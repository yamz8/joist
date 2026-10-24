// Joist — RFI and submittal logs for construction projects, with a
// ball-in-court view of who holds each item.

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "notifier.h"
#include "portalfilepicker.h"
#include "project.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // Commands and help draw no window, so they must not need a display, even
    // where the desktop exports QT_QPA_PLATFORM=wayland. Bare joist prints
    // help, as a command line tool should; the launcher says joist open.
    const QByteArray first = argc > 1 ? QByteArray(argv[1]) : QByteArray();
    const bool command = argc == 1 || isCliCommand(QString::fromLocal8Bit(first));
    bool windowless = command;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        windowless = windowless || argument == "--help" || argument == "-h" || argument == "--version" || argument == "-v";
    }
    if (windowless)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("joist");
    app.setApplicationVersion("0.1.0");

    // Matches the window to joist.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("joist");
    app.setWindowIcon(QIcon::fromTheme("joist"));

    // Installed before anything is translated, so exported PDFs and
    // notifications speak the user's language too.
    Translator translator;
    app.installTranslator(&translator);

    if (command)
        return runCli(app.arguments());

    QQuickStyle::setStyle("Material");

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "RFI and submittal logs for construction projects.")
                                     + "\n\n" + cliSummary());
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("project", QCoreApplication::translate("main", "The project to open, by name, number, or file."));
    // "joist open x" and "joist x" both open the window.
    QStringList arguments = app.arguments();
    if (arguments.value(1) == "open")
        arguments.removeAt(1);
    parser.process(arguments);

    Theme theme;
    PortalFilePicker picker;
    Notifier notifier;
    Backend backend(&picker);
    backend.setNotifier(&notifier);

    const QString named = parser.positionalArguments().value(0);
    const QString file = named.isEmpty() ? QString() : ProjectStore::find(ProjectStore::defaultDirectory(), named);
    if (!file.isEmpty())
        backend.openProject(file);
    else {
        backend.openLast();
        if (!named.isEmpty())
            backend.reportError(QCoreApplication::translate("main", "No single project matches \"%1\".").arg(named));
    }

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Once the window is up, so a missing notify-send shows in it.
    QTimer::singleShot(2000, &backend, &Backend::checkOverdue);

    return app.exec();
}
