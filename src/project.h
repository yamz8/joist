#pragma once

#include <QDate>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// A drawing sheet in a Lintel set: the PDF and the sheet's label (A-101).
struct SheetLink {
    QString file;
    QString sheet;
};

// One step in an item's life: who passed it to whom, and why. The action
// names a step that isn't a plain handoff: submitted, reopened, or how it
// closed.
struct Event {
    QDate date;
    QString from;
    QString to;
    QString note;
    QString action;
};

// An RFI or a submittal. An item is open until it has a closing date, and
// while it's open exactly one party holds it: the ball is in their court.
struct Item {
    QString type; // "rfi" or "submittal"
    int number = 0;
    int revision = 0;
    QString title;
    QString question; // The RFI's question, or what the submittal covers.
    QString answer;   // The RFI's answer, or the reviewer's comments.
    QString spec;     // Specification section, such as 03 30 00.
    QDate created;
    QDate due;
    QDate closed;
    QString ballInCourt;
    QString action; // How it closed: answered, approved, rejected...
    QList<Event> history;
    QList<SheetLink> sheets;

    QString code() const;
    bool isOpen() const { return !closed.isValid(); }
    bool isOverdue(const QDate &today) const { return isOpen() && due.isValid() && due < today; }
    // The party that raised it, who gets it back for a resubmittal.
    QString originator() const { return history.isEmpty() ? QString() : history.first().from; }

    QJsonObject toJson() const;
    static Item fromJson(const QJsonObject &object);
};

struct Project {
    QString name;
    QString number;
    QStringList parties;
    int rfiDays = 7;
    int submittalDays = 14;
    QList<Item> items;

    int indexOf(const QString &code) const;
    int nextNumber(const QString &type) const;
    int defaultDays(const QString &type) const { return type == "rfi" ? rfiDays : submittalDays; }

    QJsonObject toJson() const;
    static Project fromJson(const QJsonObject &object);
};

// Projects live one per JSON file in a directory, by default
// ~/.local/share/joist/projects, where the app, the command line, and the
// notification timer all find them.
namespace ProjectStore {
QString defaultDirectory();
QStringList files(const QString &directory);
bool load(const QString &path, Project &project, QString *error);
// Atomic: written beside the target, then renamed over it.
bool save(const QString &path, const Project &project, QString *error);
// A new, unused file name for a project in the directory.
QString pathFor(const QString &directory, const Project &project);
// A project named by its file, its name, or its number; empty if none or
// more than one matches.
QString find(const QString &directory, const QString &name);
}

// The closing actions, by type. Revise and resubmit keeps a submittal open.
QStringList closingActions(const QString &type);
bool isItemType(const QString &type);
// "RFI-7", "rfi-007", "SUB-012", or "12" with a type, to a type and number.
bool parseCode(const QString &code, QString *type, int *number);
