#pragma once

#include <QDate>
#include <QString>

#include "project.h"

// Logs and forms as PDF, drawn with QPainter so they need no display and
// the command line makes the same files as the window.
namespace PdfExport {
// kind is "rfi" or "submittal" for a log, or "board" for the open items
// grouped by who holds them.
bool writeLog(const Project &project, const QString &kind, const QDate &today, const QString &path, QString *error);
bool writeForm(const Project &project, const Item &item, const QDate &today, const QString &path, QString *error);
// A file name to suggest, such as "2417 RFI log 2026-10-24.pdf".
QString suggestedName(const Project &project, const QString &kind, const QDate &today);
QString suggestedName(const Project &project, const Item &item);
}
