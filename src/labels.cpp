#include "labels.h"

#include <QCoreApplication>

#include "project.h"

QString Labels::action(const QString &action) {
    if (action == "submitted") return QCoreApplication::translate("Labels", "Submitted");
    if (action == "reopened") return QCoreApplication::translate("Labels", "Reopened");
    if (action == "answered") return QCoreApplication::translate("Labels", "Answered");
    if (action == "withdrawn") return QCoreApplication::translate("Labels", "Withdrawn");
    if (action == "approved") return QCoreApplication::translate("Labels", "Approved");
    if (action == "approved-as-noted") return QCoreApplication::translate("Labels", "Approved as noted");
    if (action == "revise-resubmit") return QCoreApplication::translate("Labels", "Revise and resubmit");
    if (action == "rejected") return QCoreApplication::translate("Labels", "Rejected");
    return action;
}

QString Labels::type(const QString &type) {
    return type == "rfi" ? QCoreApplication::translate("Labels", "RFI")
                         : QCoreApplication::translate("Labels", "Submittal");
}

QString Labels::status(const Item &item, const QDate &today) {
    if (!item.isOpen())
        return action(item.action);
    return item.isOverdue(today) ? QCoreApplication::translate("Labels", "Overdue")
                                 : QCoreApplication::translate("Labels", "Open");
}
