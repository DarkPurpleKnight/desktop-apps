/*
 * (c) Copyright Ascensio System SIA 2010-2016
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
 * You can contact Ascensio System SIA at Lubanas st. 125a-25, Riga, Latvia,
 * EU, LV-1021.
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

#include "cmessage.h"
#include <QPushButton>
#include <QFormLayout>
#include <QLabel>
#include <QVariant>
#include <QDebug>

#include "defines.h"

#if defined(_WIN32)
# include "win/qwinwidget.h"
#endif

#define MSG_ICON_WIDTH  35
#define MSG_ICON_HEIGHT 35

extern BYTE g_dpi_ratio;
//extern QString g_lang;

CMessage::CMessage(HWND p)
    : CWinWindow(p, QString(APP_TITLE))
    , m_modalresult(MODAL_RESULT_CANCEL)
    , m_typeIcon(new QLabel)
    , m_message(new QLabel)
{
    HWND _hwnd = CWinWindow::m_hSelf;
    m_centralWidget = new QWinWidget(_hwnd);

    QVBoxLayout * _c_layout  = new QVBoxLayout;
    QHBoxLayout * _h_layout2 = new QHBoxLayout;
    QHBoxLayout * _h_layout1 = new QHBoxLayout;
    _c_layout->addLayout(_h_layout2, 1);
    _c_layout->addLayout(_h_layout1, 0);

    m_typeIcon->setProperty("class", "msg-icon");
    m_typeIcon->setFixedSize(MSG_ICON_WIDTH*g_dpi_ratio, MSG_ICON_HEIGHT*g_dpi_ratio);
    _h_layout2->addWidget(m_typeIcon, 0, Qt::AlignTop);

//    m_message->setWordWrap(true);
    m_message->setStyleSheet(QString("margin-bottom: %1px;").arg(8*g_dpi_ratio));

    QFormLayout * _f_layout = new QFormLayout;
    _f_layout->addWidget(m_message);
    _f_layout->setSpacing(0);
    _f_layout->setContentsMargins(10*g_dpi_ratio,0,5*g_dpi_ratio,0);
    _h_layout2->addLayout(_f_layout, 1);

    _h_layout2->setContentsMargins(15,10,15,10);
    _f_layout->setContentsMargins(10,0,0,0);

    QPushButton * btn_ok = new QPushButton(QObject::tr("&OK"));
    m_boxButtons = new QWidget;
    m_boxButtons->setLayout(new QHBoxLayout);
    m_boxButtons->layout()->addWidget(btn_ok);
    m_boxButtons->layout()->setContentsMargins(0,8*g_dpi_ratio,0,0);
    _h_layout1->addWidget(m_boxButtons, 0, Qt::AlignCenter);

    QObject::connect(btn_ok, &QPushButton::clicked,
        [=] {
            m_modalresult = MODAL_RESULT_YES;
#if defined(_WIN32)
            DestroyWindow(_hwnd);
#else
            close();
#endif
        }
    );

    m_centralWidget->setLayout(_c_layout);
    m_centralWidget->setMinimumWidth(350*g_dpi_ratio);
//    m_centralWidget->setWindowTitle(APP_TITLE);
    m_centralWidget->setStyleSheet("QPushButton{min-width:40px;}");
    m_centralWidget->move(0, 0);
}

void CMessage::setButtons(std::initializer_list<QString> btns)
{
    foreach (QWidget * w, m_boxButtons->findChildren<QWidget*>()) {
        w->disconnect();
        delete w;
    }

    HWND _hwnd = CWinWindow::m_hSelf;
    QRegExp reFocus("([^:]+)\\:?(default)?$");

    QPushButton * _btn;
    int _btn_num(0);
    int * _result = &m_modalresult;
    for (auto btn: btns) {
        reFocus.indexIn(btn);

        _btn = new QPushButton(reFocus.cap(1));
        if ( !reFocus.cap(2).isEmpty() ) {
            _btn->setDefault(true);
        }

        m_boxButtons->layout()->addWidget(_btn);
        QObject::connect(_btn, &QPushButton::clicked, [_hwnd, _btn_num, _result](){
            *_result = MODAL_RESULT_CUSTOM + _btn_num;
            DestroyWindow(_hwnd);
        });

        _btn_num++;
    }

    if (_btn_num > 2)
        m_centralWidget->setMinimumWidth(400*g_dpi_ratio);
}

int CMessage::info(const QString& mess)
{
    m_message->setText(mess);
    m_typeIcon->setProperty("type","msg-info");

    modal();

    return m_modalresult;
}

int CMessage::warning(const QString& mess)
{
    m_message->setText(mess);
    m_typeIcon->setProperty("type","msg-warn");

    modal();

    return m_modalresult;
}

int CMessage::error(const QString& mess)
{
    m_message->setText(mess);
    m_typeIcon->setProperty("type","msg-error");

    modal();

    return m_modalresult;
}

int CMessage::confirm(const QString& mess)
{
    m_message->setText(mess);
    m_typeIcon->setProperty("type","msg-confirm");

    modal();

    return m_modalresult;
}

int CMessage::confirm(HWND p, const QString& m)
{
    CMessage mess(p);
    return mess.confirm(m);
}

int CMessage::info(HWND p, const QString& m)
{
    CMessage mess(p);
    return mess.info(m);
}

int CMessage::warning(HWND p, const QString& m)
{
    CMessage mess(p);
    return mess.warning(m);
}

int CMessage::error(HWND p, const QString& m)
{
    CMessage mess(p);
    return mess.error(m);
}

void CMessage::modal()
{
    m_centralWidget->adjustSize();

#if defined(_WIN32)
    m_centralWidget->show();

    CWinWindow::setSize(m_centralWidget->width(), m_centralWidget->height());
    CWinWindow::center();
    CWinWindow::modal();
#else
    layout()->setSizeConstraint(QLayout::SetFixedSize);
#endif
}
