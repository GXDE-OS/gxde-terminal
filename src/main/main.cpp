// Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "environments.h"
#include "dbusmanager.h"
#include "service.h"
#include "utils.h"
#include "terminalapplication.h"
#include "define.h"


#include <DApplication>
#include <DPlatformWindowHandle>
#include <DLog>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <DApplicationSettings>
#endif

#include <QDir>
#include <QDebug>
#include <QProcess>
#include <QCommandLineParser>
#include <QTranslator>
#include <QTime>
#include <QElapsedTimer>
#include <QUrl>
#include <QMenu>
#include <QStyle>
#include <QStyleFactory>
#include <QProxyStyle>
#include <QPainter>
#ifdef HAVE_KWINDOWEFFECTS
#include <KWindowEffects>
#endif

DWIDGET_USE_NAMESPACE

DCORE_USE_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(mainprocess)

namespace {
class GxdeMenuProxyStyle : public QProxyStyle
{
public:
    explicit GxdeMenuProxyStyle(QStyle *style) : QProxyStyle(style) {}

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override
    {
        painter->save();
        if (element == PE_PanelMenu && widget && widget->property("gxdeMenuBlurFallback").toBool())
            painter->setOpacity(painter->opacity() * 0.6);
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        painter->restore();
    }
};

class GxdeMenuStyle : public QObject
{
public:
    explicit GxdeMenuStyle(QApplication *app) : QObject(app) {
        for (const QString &name : {QStringLiteral("ddark2"),
                QStringLiteral("dlight2")}) {
            if (auto base = QStyleFactory::create(name)) {
                auto style = new GxdeMenuProxyStyle(base);
                style->setObjectName(name);
                style->setParent(this);
                if (name == "ddark2") m_dark = style;
                else m_light = style;
            }
        }

        app->installEventFilter(this);
        connect(DGuiApplicationHelper::instance(),
                &DGuiApplicationHelper::themeTypeChanged, this, [this] {
            for (auto widget : QApplication::allWidgets())
                if (auto menu = qobject_cast<QMenu *>(widget)) applyTheme(menu);
        });

#ifdef HAVE_KWINDOWEFFECTS
        KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
#endif
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
            if (auto menu = qobject_cast<QMenu *>(object)) {
                applyTheme(menu);
                // DDark2's own menu setup is guarded by isDXcbPlatform(),
                // so Wayland menus need the translucent surface and blur request here.
                if (QGuiApplication::platformName().startsWith("wayland")) {
                    menu->setAttribute(Qt::WA_TranslucentBackground);
                    DPlatformWindowHandle handle(menu);
#ifdef HAVE_KWINDOWEFFECTS
                    const bool fallback = KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
                    menu->setProperty("gxdeMenuBlurFallback", fallback);
                    if (fallback)
                        KWindowEffects::enableBlurBehind(menu->windowHandle());
                    else
#endif
                        handle.setEnableBlurWindow(true);
                }
            }
        }
        return false;
    }

private:
    void applyTheme(QMenu *menu) {
        auto style = DGuiApplicationHelper::instance()
            ->themeType() == DGuiApplicationHelper::LightType ?
                m_light : m_dark;

        if (!style) {
            return;
        }

        if (menu->style() != style) {
            menu->setStyle(style);
        }

        QPalette palette = style->standardPalette();
        style->polish(palette);
        const bool dark = style == m_dark;
        for (auto group : {QPalette::Active, QPalette::Inactive,
                QPalette::Disabled}) {
            const QColor text = group == QPalette::Disabled
                ? QColor(dark ? "#909090" : "#808080")
                : QColor(dark ? "#eeeeee" : "#252525");

            for (auto role : {QPalette::WindowText, QPalette::Text,
                    QPalette::ButtonText}) {
                palette.setColor(group, role, text);
            }

            palette.setColor(group, QPalette::Window, QColor(
                dark ? "#252525" : "#f5f5f5"));
            palette.setColor(group, QPalette::Base, QColor(
                dark ? "#252525" : "#f5f5f5"));
        }

        menu->setPalette(palette);
        menu->update();
    }

    QStyle *m_dark = nullptr;
    QStyle *m_light = nullptr;
};
}

bool checkImmutableMode() {
    QProcess process;
    QStringList arguments;
    arguments << "-s";  // 命令参数

    // 启动进程并等待其完成
    process.start("deepin-immutable-ctl", arguments);
    if (!process.waitForStarted()) {
        qWarning() << "Failed to start deepin-immutable-ctl:" << process.errorString();
        return false;
    }

    if (!process.waitForFinished()) {
        qWarning() << "deepin-immutable-ctl did not finish successfully:" << process.errorString();
        return false;
    }

    // 获取命令输出
    QByteArray output = process.readAllStandardOutput();
    QString result = QString::fromUtf8(output.trimmed());
    if (result.split(":").at(1) == "true") {
        qInfo() << "System is in immutable mode.";
        return true;
    } else {
        qInfo() << "System is not in immutable mode. Output was:" << result;
        return false;
    }
}

int main(int argc, char *argv[])
{
    qCDebug(mainprocess) << "Application starting with arguments:" << QCoreApplication::arguments();
    if (checkImmutableMode())
        setenv("PWD", getenv("HOME"), 1);
    if (!QString(qgetenv("XDG_CURRENT_DESKTOP")).toLower().startsWith("deepin")) {
        setenv("XDG_CURRENT_DESKTOP", "Deepin", 1);
    }
    // 应用计时
    QTime useTime;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    useTime.start();
#endif
    //为了更精准，起动就度量时间
    qint64 startTime = QDateTime::currentDateTime().toMSecsSinceEpoch();

    // 启动应用
    qCDebug(mainprocess) << "Creating TerminalApplication instance";
    TerminalApplication app(argc, argv);
    new GxdeMenuStyle(&app);
    app.setStartTime(startTime);
    qCInfo(mainprocess) << "TerminalApplication initialized, start time:" << startTime;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    DApplicationSettings set(&app);
#endif

    // 系统日志
#if (DTK_VERSION >= DTK_VERSION_CHECK(5,6,8,0))
    //qCDebug(mainprocess) << "current libdtkcore5 > 5.6.8.0";
    DLogManager::registerJournalAppender();
    DLogManager::registerFileAppender();
    //qCInfo(mainprocess) << "Current log register journal!";
#ifdef QT_DEBUG
    DLogManager::registerConsoleAppender();
    //qCInfo(mainprocess) << "Current log register console!";
#endif
#else
    qCDebug(mainprocess) << "current libdtkcore5 < 5.6.8.0";
//    DLogManager::registerJournalAppender();
    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();
    qCInfo(mainprocess) << "Current log register console and file!";
#endif

#ifdef DTKCORE_CLASS_DConfigFile
    //qCInfo(mainprocess) << "DConfig is supported by DTK";
    //日志规则
    LoggerRules logRules;
    logRules.initLoggerRules();
#endif

    // 参数解析
    qCDebug(mainprocess) << "Parsing command line arguments";
    TermProperties properties;
    QStringList args = app.arguments();

    for (int i = 0; i < args.size(); i++) {
        if (args[i].startsWith("dsg-terminal-exec://")) {
            QString execCMD = args[i];
            execCMD.remove("dsg-terminal-exec://");
            args += "-e";
            args += QUrl::fromPercentEncoding(execCMD.toUtf8());
            break;
        }
    }

    Utils::parseCommandLine(app.arguments(), properties, true);

    if(!(args.contains("-w") || args.contains("--work-directory"))) {
        args += "-w";
        args += QDir::currentPath();
    }

    qCDebug(mainprocess) << "Initializing DBus manager";
    DBusManager manager;
    if (!manager.initDBus()) {
      // 非第一次启动，尝试通过 DBus 调用已存在的终端实例
      qCInfo(mainprocess) << "DBus service registration failed, trying to call "
                             "existing terminal instance...";

      if (DBusManager::callTerminalEntry(args)) {
        qCInfo(mainprocess)
            << "Successfully called existing terminal instance via DBus.";
        return 0;
      } else {
        // DBus 调用失败，可能是服务不可用，提供备用方案
        qCWarning(mainprocess)
            << "Failed to call existing terminal instance via DBus!";
        qCWarning(mainprocess)
            << "Starting new terminal instance as fallback...";

        // 继续执行正常的终端启动流程，作为备用方案
        // 注意：这里不能再次尝试初始化 DBus 服务，因为服务名可能仍被占用
        // 但我们可以启动一个独立的终端实例
      }
    }
    // 第一次启动
    qCInfo(mainprocess) << "First terminal instance starting";
    // 这行不要删除
    qputenv("TERM", "xterm-256color");
    // 首次启动
    qCDebug(mainprocess) << "Setting up DBus signal connections";
    QObject::connect(&manager, &DBusManager::entryArgs, Service::instance(), &Service::Entry);
    qCDebug(mainprocess) << "Starting terminal service";
    Service::instance()->EntryTerminal(args);
    qCInfo(mainprocess) << "Terminal service started successfully";
    // 监听触控板事件
    qCDebug(mainprocess) << "Setting up touchpad signal listener";
    manager.listenTouchPadSignal();
    qCInfo(mainprocess) << "Application initialization completed";

    return app.exec();
}
