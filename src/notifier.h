#pragma once

#include <QDate>
#include <QList>
#include <QObject>
#include <QString>

struct Overdue {
    QString file;
    QString project;
    QString code;
    QString title;
    QString holder;
    int daysLate = 0;
};

// Sends a desktop notification for each overdue item, once a day per item,
// whether the window is open or not: the app checks while it runs, and
// `joist notify` runs from a systemd user timer. Both share one state file,
// so an item is never announced twice in a day.
class Notifier : public QObject {
    Q_OBJECT

public:
    explicit Notifier(const QString &stateFile = {}, const QString &command = "notify-send",
                      QObject *parent = nullptr);

    static QList<Overdue> overdueItems(const QStringList &projectFiles, const QDate &today);
    // The overdue items not yet announced today.
    QList<Overdue> pending(const QList<Overdue> &overdue, const QDate &today) const;
    // Announces the pending items and remembers them; false with an error
    // when notify-send is missing or fails.
    bool notify(const QList<Overdue> &overdue, const QDate &today, QString *error);

    static QString lateText(int days);

private:
    bool send(const QString &summary, const QString &body, QString *error);

    QString m_stateFile;
    QString m_command;
};
