#include "notifier.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

#include "project.h"

namespace {

QJsonObject readState(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

QString key(const Overdue &item) { return item.file + "#" + item.code; }

// More than this many at once arrive as one summary, not a flood.
constexpr int SummaryThreshold = 3;

} // namespace

Notifier::Notifier(const QString &stateFile, const QString &command, QObject *parent)
    : QObject(parent),
      m_stateFile(stateFile.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/joist/notified.json"
                      : stateFile),
      m_command(command) {}

QList<Overdue> Notifier::overdueItems(const QStringList &projectFiles, const QDate &today) {
    QList<Overdue> overdue;
    for (const QString &path : projectFiles) {
        Project project;
        if (!ProjectStore::load(path, project, nullptr))
            continue;
        for (const Item &item : project.items)
            // Items due today are announced too, while there's still time.
            if (item.isOpen() && item.due.isValid() && item.due <= today)
                overdue.append({path, project.name, item.code(), item.title, item.ballInCourt,
                                static_cast<int>(item.due.daysTo(today))});
    }
    std::stable_sort(overdue.begin(), overdue.end(),
                     [](const Overdue &a, const Overdue &b) { return a.daysLate > b.daysLate; });
    return overdue;
}

QList<Overdue> Notifier::pending(const QList<Overdue> &overdue, const QDate &today) const {
    const QJsonObject state = readState(m_stateFile);
    const QString date = today.toString(Qt::ISODate);
    QList<Overdue> result;
    for (const Overdue &item : overdue)
        if (state.value(key(item)).toString() != date)
            result.append(item);
    return result;
}

QString Notifier::lateText(int days) {
    return days == 0 ? tr("Due today") : tr("%n days overdue", nullptr, days);
}

bool Notifier::notify(const QList<Overdue> &overdue, const QDate &today, QString *error) {
    const QList<Overdue> due = pending(overdue, today);
    if (due.isEmpty())
        return true;

    bool sent = true;
    if (due.size() > SummaryThreshold) {
        QStringList lines;
        for (const Overdue &item : due.mid(0, 6))
            lines.append(tr("%1 %2 · %3").arg(item.code, item.holder, lateText(item.daysLate)));
        if (due.size() > 6)
            lines.append(tr("and %n more", nullptr, int(due.size() - 6)));
        sent = send(tr("%n overdue items", nullptr, int(due.size())), lines.join("\n"), error);
    } else {
        for (const Overdue &item : due) {
            const QString summary = item.daysLate == 0 ? tr("%1 is due today").arg(item.code)
                                                       : tr("%1 is %n days overdue", nullptr, item.daysLate).arg(item.code);
            const QString body = tr("%1\nBall in court: %2\n%3").arg(item.title, item.holder, item.project);
            sent = send(summary, body, error) && sent;
        }
    }
    if (!sent)
        return false;

    // Today's announcements replace the old state, which also forgets items
    // that have since been closed.
    QJsonObject state = readState(m_stateFile);
    const QString date = today.toString(Qt::ISODate);
    for (auto it = state.begin(); it != state.end();)
        it = it.value().toString() == date ? std::next(it) : state.erase(it);
    for (const Overdue &item : due)
        state.insert(key(item), date);
    QDir().mkpath(QFileInfo(m_stateFile).absolutePath());
    QSaveFile file(m_stateFile);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(state).toJson()) < 0 || !file.commit()) {
        if (error)
            *error = tr("Could not save %1: %2").arg(m_stateFile, file.errorString());
        return false;
    }
    return true;
}

bool Notifier::send(const QString &summary, const QString &body, QString *error) {
    QProcess process;
    process.start(m_command, {"--app-name=Joist", "--icon=joist", "--urgency=critical", summary, body});
    if (!process.waitForStarted(3000)) {
        if (error)
            *error = tr("Could not send a notification: %1 is not installed.").arg(m_command);
        return false;
    }
    if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error)
            *error = tr("Could not send a notification: %1").arg(QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
        return false;
    }
    return true;
}
