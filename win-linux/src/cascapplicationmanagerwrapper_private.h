/*
 * (c) Copyright Ascensio System SIA 2010-2019
 *
 * This program is a free software product. You can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License (AGPL)
 * version 3 as published by the Free Software Foundation. In accordance with
 * Section 7(a) of the GNU AGPL its Section 15 shall be amended to the effect
 * that Ascensio System SIA expressly excludes the warranty of non-infringement
 * of any third-party rights.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR  PURPOSE. For
 * details, see the GNU AGPL at: http://www.gnu.org/licenses/agpl-3.0.html
 *
 * You can contact Ascensio System SIA at 20A-12 Ernesta Birznieka-Upisha
 * street, Riga, Latvia, EU, LV-1050.
 *
 * The  interactive user interfaces in modified source and object code versions
 * of the Program must display Appropriate Legal Notices, as required under
 * Section 5 of the GNU AGPL version 3.
 *
 * Pursuant to Section 7(b) of the License you must retain the original Product
 * logo when distributing the program. Pursuant to Section 7(e) we decline to
 * grant you any rights under trademark law for use of our trademarks.
 *
 * All the Product's GUI elements, including illustrations and icon sets, as
 * well as technical writing content are licensed under the terms of the
 * Creative Commons Attribution-ShareAlike 4.0 International. See the License
 * terms at http://creativecommons.org/licenses/by-sa/4.0/legalcode
 *
*/

#ifndef CASCAPPLICATIONMANAGERWRAPPER_PRIVATE_H
#define CASCAPPLICATIONMANAGERWRAPPER_PRIVATE_H

#include "cascapplicationmanagerwrapper.h"
#include "qcefview_media.h"
#include "defines.h"
#include "clangater.h"
#include "cappeventfilter.h"
#include "OfficeFileFormats.h"
#include "ceditortools.h"
#include "utils.h"
#include "cmessage.h"


class CAscApplicationManagerWrapper_Private
{
public:
    CAscApplicationManagerWrapper_Private(CAscApplicationManagerWrapper * manager)
        : m_appmanager(*manager)
    {
#ifdef Q_OS_WIN
        qApp->installEventFilter(new CAppEventFilter(this));
#endif

        GET_REGISTRY_USER(reg_user);
        m_openEditorWindow = reg_user.value("editorWindowMode").toBool();
    }

    virtual ~CAscApplicationManagerWrapper_Private() {}

    virtual void initializeApp() {}
    virtual bool processEvent(NSEditorApi::CAscCefMenuEvent * event) {
        if ( detectDocumentOpening(*event) )
            return true;

        return false;
    }
    virtual void applyStylesheets() {}
    virtual void addStylesheets(CScalingFactor f, const std::string& s)
    {
        m_appmanager.addStylesheets(f, s);
    }

    virtual QCefView * createView(QWidget * parent)
    {
        return new QCefView_Media(parent);
    }

    bool allowedCreateLocalFile()
    {
        return true;
    }

    virtual void init()
    {
    }

    auto createStartPanel() -> void {
        GET_REGISTRY_USER(reg_user)

        m_pStartPanel = AscAppManager::createViewer(nullptr);
        m_pStartPanel->Create(&m_appmanager, cvwtSimple);
        m_pStartPanel->setObjectName("mainPanel");

        QString data_path;
#if defined(QT_DEBUG)
        data_path = reg_user.value("startpage").value<QString>();
#endif

        if ( data_path.isEmpty() )
            data_path = qApp->applicationDirPath() + "/index.html";

        QString additional = "?waitingloader=yes&lang=" + CLangater::getCurrentLangCode();
        QString _portal = reg_user.value("portal").value<QString>();
        if (!_portal.isEmpty()) {
            QString arg_portal = (additional.isEmpty() ? "?portal=" : "&portal=") + _portal;
            additional.append(arg_portal);
        }

        std::wstring start_path = ("file:///" + data_path + additional).toStdWString();
        m_pStartPanel->GetCefView()->load(start_path);
    }

    auto handleAppKeyPress(QKeyEvent * e) -> bool
    {
        if (e->key() == Qt::Key_O && (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
            NSEditorApi::CAscCefMenuEvent * ns_event = new NSEditorApi::CAscCefMenuEvent(ASC_MENU_EVENT_TYPE_CEF_EXECUTE_COMMAND);
            NSEditorApi::CAscExecCommand * pData = new NSEditorApi::CAscExecCommand;
            pData->put_Command(L"open:folder");

            ns_event->m_pData = pData;
            m_appmanager.OnEvent(ns_event);

            return true;
        }

        return false;
    }

    auto preferOpenEditorWindow() -> bool
    {
        return m_openEditorWindow;
    }

    auto detectDocumentOpening(const NSEditorApi::CAscCefMenuEvent& event) -> bool
    {
        switch ( event.m_nType ) {
        case ASC_MENU_EVENT_TYPE_CEF_CREATETAB: {
            NSEditorApi::CAscCreateTab & data = *static_cast<NSEditorApi::CAscCreateTab *>(event.m_pData);
//            data.get_Active();

            COpenOptions opts{data.get_Url()};
            opts.id = data.get_IdEqual();
            opts.parent_id = event.m_nSenderId;

            if ( CCefView * _v = m_appmanager.GetViewById(opts.id) ) {
                bringEditorToFront(_v->GetId());
            } else openDocument(opts);

            return true;
        }
        case ASC_MENU_EVENT_TYPE_CEF_EXECUTE_COMMAND: {
            NSEditorApi::CAscExecCommand & data = *static_cast<NSEditorApi::CAscExecCommand *>(event.m_pData);
            const std::wstring & cmd = data.get_Command();

            if ( cmd.compare(L"open:recent") == 0 ) {
                QJsonObject objRoot = Utils::parseJson(data.get_Param());
                if ( !objRoot.isEmpty() ) {
                    COpenOptions opts{objRoot["path"].toString().toStdWString(), etRecentFile, objRoot["id"].toInt()};
                    opts.format = objRoot["type"].toInt();
                    opts.parent_id = event.m_nSenderId;

                    QRegularExpression re(rePortalName);
                    QRegularExpressionMatch match = re.match(opts.url);

                    if ( !match.hasMatch() ) {
                        QFileInfo _info(opts.url);
                        if ( /*!data->get_IsRecover() &&*/ !_info.exists() ) {
                            CMessage mess(m_appmanager.mainWindow()->handle(), CMessageOpts::moButtons::mbYesDefNo);
                            int modal_res = mess.warning(QObject::tr("%1 doesn't exists!<br>Remove file from the list?").arg(_info.fileName()));

                            if ( modal_res == MODAL_RESULT_CUSTOM ) {
                                AscAppManager::sendCommandTo(SEND_TO_ALL_START_PAGE, "file:skip", QString::number(opts.id));
                                return true;
                            }
                        }
                    }

                    openDocument(opts);
                }

                return true;
            } else
            if ( cmd.compare(L"open:document") == 0 ) {
                const std::wstring & _url = data.get_Param();
                if ( !_url.empty() ) {
                    CCefView * _view = m_appmanager.GetViewByUrl(_url);
                    if ( _url.rfind(L"http://",0) == 0 || _url.rfind(L"https://",0) == 0 ) {
                        COpenOptions opts{_url};
                        opts.id = _view ? _view->GetId() : -1;
//                        mainWindow()->mainPanel()->onCloudDocumentOpen(_url, _id, true);
                    } else {
//                        /* open local file */
                    }
                }
            } else
            if ( cmd.compare(L"open:folder") == 0 ) {
                std::wstring file_path = CEditorTools::getlocalfile(data.get_Param()).toStdWString();

                if ( !file_path.empty() ) {
                    CCefView * _view = m_appmanager.GetViewByUrl(file_path);

                    if ( _view ) {
                        bringEditorToFront(_view->GetId());
                    } else {
                        COpenOptions opts{file_path, etLocalFile};
                        opts.parent_id = event.m_nSenderId;

                        if ( !openDocument(opts) ) {
                            QFileInfo _info(QString::fromStdWString(file_path));
                            CMessage mess(m_appmanager.mainWindow()->handle());
                            mess.error(QObject::tr("File %1 cannot be opened or doesn't exists.").arg(_info.fileName()));
                        }
                    }
                }

                return true;
            } else
            if ( cmd.compare(L"create:new") == 0 ) {
                const std::wstring & format = data.get_Param();
                int _f = format == L"word" ? AVS_OFFICESTUDIO_FILE_DOCUMENT_DOCX :
                            format == L"cell" ? AVS_OFFICESTUDIO_FILE_SPREADSHEET_XLSX :
                            format == L"oform" ? AVS_OFFICESTUDIO_FILE_DOCUMENT_OFORM :
                            format == L"slide" ? AVS_OFFICESTUDIO_FILE_PRESENTATION_PPTX : AVS_OFFICESTUDIO_FILE_UNKNOWN;

                COpenOptions opts{m_appmanager.newFileName(_f), etNewFile};
                opts.format = _f;
                opts.parent_id = event.m_nSenderId;

                openDocument(opts);
                return true;
            }

        }
        }
        return false;
    }

    auto bringEditorToFront(int viewid) -> void
    {
        CEditorWindow * editor = m_appmanager.editorWindowFromViewId(viewid);
        if ( editor )
            editor->bringToTop();
        else m_appmanager.mainWindow()->selectView(viewid);
    }

    auto windowRectFromViewId(int viewid) -> QRect
    {
        if ( !(viewid < 0) ) {
            CEditorWindow * editor = m_appmanager.editorWindowFromViewId(viewid);
            if ( editor )
                return editor->geometry();
            else
            if ( m_appmanager.mainWindow() && m_appmanager.mainWindow()->holdView(viewid) )
                return m_appmanager.mainWindow()->windowRect();
        }

        return QRect();
    }

    auto openDocument(const COpenOptions& opts) -> bool
    {
        CTabPanel * panel = CEditorTools::createEditorPanel(opts);
        if ( panel ) {
            CAscTabData * panel_data = panel->data();
            QRegularExpression re("ascdesktop:\\/\\/compare");

            if ( re.match(QString::fromStdWString(panel_data->url())).hasMatch() ) {
                 panel_data->setIsLocal(true);
                 panel_data->setUrl("");
            }

            if ( preferOpenEditorWindow() ) {
                QRect rect = windowRectFromViewId(opts.parent_id);
                if ( !rect.isEmpty() )
                    rect.adjust(50,50,50,50);

                CEditorWindow * editor_win = new CEditorWindow(rect, panel);
                editor_win->show(false);

                m_appmanager.m_vecEditors.push_back(size_t(editor_win));
                m_appmanager.sendCommandTo(panel->cef(), L"window:features", Utils::stringifyJson(QJsonObject{{"skiptoparea", TOOLBTN_HEIGHT}}).toStdWString());
            } else {
                mainWindow()->attachEditor(panel);
            }

            return true;
        }

        return false;

    }

protected:
    auto mainWindow() -> CMainWindow * {
        return m_appmanager.m_pMainWindow;
    }

public:
    CAscApplicationManagerWrapper& m_appmanager;
    QPointer<QCefView> m_pStartPanel;
    bool m_openEditorWindow = false;
};

//CAscApplicationManagerWrapper::CAscApplicationManagerWrapper()
//    : CAscApplicationManagerWrapper(new CAscApplicationManagerWrapper::CAscApplicationManagerWrapper_Private(this))
//{
//}

#endif // CASCAPPLICATIONMANAGERWRAPPER_PRIVATE_H
