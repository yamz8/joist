#pragma once

#include <QDate>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <functional>

#include "project.h"

class FilePicker;
class Notifier;

// The open project, its items, and every change to them, for the window.
// Every change saves at once and can be undone; a change made outside the
// app (by `joist pass` from a script, say) reloads the project.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList projects READ projects NOTIFY projectsChanged)
    Q_PROPERTY(bool hasProject READ hasProject NOTIFY projectChanged)
    Q_PROPERTY(QString projectFile READ projectFile NOTIFY projectChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)
    Q_PROPERTY(QString projectNumber READ projectNumber NOTIFY projectChanged)
    Q_PROPERTY(QStringList parties READ parties NOTIFY projectChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY projectChanged)
    Q_PROPERTY(QVariantList board READ board NOTIFY projectChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY projectChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY projectChanged)
    Q_PROPERTY(QString today READ todayText NOTIFY projectChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool listingSheets READ listingSheets NOTIFY listingSheetsChanged)

public:
    explicit Backend(FilePicker *picker, const QString &directory = {}, QObject *parent = nullptr);

    // For tests: a fixed day, a fake lintel, and where notifications go.
    void setToday(const QDate &today);
    void setLintelCommand(const QString &command) { m_lintel = command; }
    void setNotifier(Notifier *notifier);

    QDate today() const { return m_today.isValid() ? m_today : QDate::currentDate(); }
    QString todayText() const { return today().toString(Qt::ISODate); }
    QVariantList projects() const { return m_projects; }
    bool hasProject() const { return !m_file.isEmpty(); }
    QString projectFile() const { return m_file; }
    QString projectName() const { return m_project.name; }
    QString projectNumber() const { return m_project.number; }
    QStringList parties() const { return m_project.parties; }
    QVariantList items() const;
    QVariantList board() const;
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString error() const { return m_error; }
    bool listingSheets() const { return m_listingSheets; }
    const Project &project() const { return m_project; }

    // Opens the project used last time, or the only one there is.
    void openLast();

    Q_INVOKABLE bool createProject(const QString &name, const QString &number);
    // remember makes it the project the window opens with next time.
    Q_INVOKABLE bool openProject(const QString &file, bool remember = true);
    Q_INVOKABLE void renameProject(const QString &name, const QString &number);

    Q_INVOKABLE bool addParty(const QString &name);
    Q_INVOKABLE bool removeParty(const QString &name);
    Q_INVOKABLE bool renameParty(const QString &from, const QString &to);
    Q_INVOKABLE void moveParty(const QString &name, int step);

    // Returns the new item's code, or empty. Without a route it goes from the
    // first party to the second, due after the project's usual days.
    Q_INVOKABLE QString addItem(const QString &type, const QString &title = {}, const QString &from = {},
                                const QString &to = {}, int days = -1);
    Q_INVOKABLE QVariantMap item(const QString &code) const;
    Q_INVOKABLE void setField(const QString &code, const QString &field, const QString &value);
    Q_INVOKABLE void shiftDue(const QString &code, int days);
    Q_INVOKABLE void passTo(const QString &code, const QString &party, int days, const QString &note);
    // Closes with an action, or sends a submittal back for revision.
    Q_INVOKABLE void closeItem(const QString &code, const QString &action, const QString &answer, const QString &note);
    Q_INVOKABLE void reopen(const QString &code);
    Q_INVOKABLE void deleteItem(const QString &code);
    // The party an item should go to next: back to whoever sent it.
    Q_INVOKABLE QString suggestedRecipient(const QString &code) const;

    Q_INVOKABLE QStringList closingActions(const QString &type) const;
    Q_INVOKABLE QString actionLabel(const QString &action) const;
    Q_INVOKABLE int defaultDays(const QString &type) const { return m_project.defaultDays(type); }

    // Picks a drawing set, then asks Lintel for its sheets: sheetsListed.
    Q_INVOKABLE void linkSheet(const QString &code);
    Q_INVOKABLE void addSheetLink(const QString &code, const QString &file, const QString &sheet);
    Q_INVOKABLE void removeSheetLink(const QString &code, int index);
    Q_INVOKABLE void openSheet(const QString &file, const QString &sheet);

    // kind is "rfi", "submittal", or "board".
    Q_INVOKABLE void exportLog(const QString &kind);
    Q_INVOKABLE void exportForm(const QString &code);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void reportError(const QString &message);
    // Announces overdue items across every project.
    Q_INVOKABLE void checkOverdue();
    // Status words come from C++, so a new language re-renders the items.
    void retranslate() { emit projectChanged(); }

signals:
    void projectsChanged();
    void projectChanged();
    void errorChanged();
    void listingSheetsChanged();
    void sheetsListed(const QString &code, const QString &file, const QStringList &sheets);
    void exported(const QString &path);
    void itemAdded(const QString &code);

private:
    bool change(const std::function<bool(Project &)> &edit);
    Item *find(Project &project, const QString &code);
    bool save();
    void refreshProjects();
    void watch();
    void reloadFromDisk();
    void listSheets(const QString &code, const QString &file);
    void saveTo(const QUrl &url);
    QVariantMap itemMap(const Item &item) const;

    FilePicker *m_picker;
    Notifier *m_notifier = nullptr;
    QString m_directory;
    QString m_file;
    Project m_project;
    QByteArray m_saved;
    QList<QJsonObject> m_undo, m_redo;
    QVariantList m_projects;
    QDate m_today;
    QString m_error;
    QString m_lintel = "lintel";
    bool m_listingSheets = false;
    int m_lintelJumps = -1; // Whether lintel open takes --sheet; -1 until asked.
    QString m_pendingLink;   // The item a picked PDF links to.
    QString m_pendingExport; // "rfi", "submittal", "board", or an item's code.
    QFileSystemWatcher m_watcher;
    QTimer m_reload;
    QTimer m_clock;
    QTimer m_overdue;
};
