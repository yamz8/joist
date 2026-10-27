#pragma once

#include <QDate>
#include <QString>

struct Item;

// Words for the stored values, in the user's language. The stored values
// themselves stay English, so projects read the same in every language.
namespace Labels {
QString action(const QString &action);
QString type(const QString &type);
// "Open", "Overdue", or how it closed.
QString status(const Item &item, const QDate &today);
QString status(bool open, bool overdue, const QString &action);
}
