#include "pdfexport.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QTextLayout>
#include <algorithm>
#include <functional>

#include "labels.h"

namespace {

QString tr(const char *text, int n = -1) { return QCoreApplication::translate("PdfExport", text, nullptr, n); }

QString date(const QDate &value) {
    return value.isValid() ? QLocale().toString(value, QLocale::ShortFormat) : QString();
}

const QColor Ink(0x1a, 0x1a, 0x1e);
const QColor Muted(0x6b, 0x6b, 0x74);
const QColor Rule(0xc8, 0xc8, 0xce);
const QColor Shade(0xf1, 0xf1, 0xf4);
const QColor Late(0xc0, 0x1c, 0x28);

// US construction prints on Letter, everyone else on A4.
QPageSize paperSize() {
    return QLocale().measurementSystem() == QLocale::ImperialUSSystem ? QPageSize(QPageSize::Letter)
                                                                       : QPageSize(QPageSize::A4);
}

// One PDF, laid out top to bottom in points, starting a new page with the
// running header whenever the next block would not fit.
class Document {
public:
    Document(QIODevice *device, QPageLayout::Orientation orientation, const Project &project, const QString &title,
             const QDate &today)
        : m_writer(device), m_project(project), m_title(title), m_today(today) {
        m_writer.setResolution(72);
        m_writer.setPageSize(paperSize());
        m_writer.setPageOrientation(orientation);
        m_writer.setPageMargins(QMarginsF(40, 36, 40, 36), QPageLayout::Point);
        m_writer.setTitle(title);
        m_writer.setCreator("Joist");
        m_painter.begin(&m_writer);
        m_width = m_writer.width();
        m_bottom = m_writer.height() - 22;
        startPage();
    }

    bool finish() { return m_painter.end(); }

    QPainter &painter() { return m_painter; }
    qreal width() const { return m_width; }
    qreal &y() { return m_y; }

    QFont font(qreal size, bool bold = false) const {
        QFont font("Sans");
        font.setPointSizeF(size);
        font.setBold(bold);
        return font;
    }

    QFontMetricsF metrics(const QFont &font) const { return QFontMetricsF(font, &m_writer); }

    // Starts a new page unless the height still fits on this one; true when
    // it did.
    bool ensure(qreal height) {
        if (m_y + height <= m_bottom)
            return false;
        m_writer.newPage();
        startPage();
        return true;
    }

    void text(const QRectF &rect, const QString &text, const QFont &font, const QColor &color = Ink,
              int flags = Qt::AlignLeft | Qt::AlignVCenter) {
        m_painter.setFont(font);
        m_painter.setPen(color);
        m_painter.drawText(rect, flags, text);
    }

    void rule(qreal y, const QColor &color = Rule, qreal width = 0.6) {
        m_painter.setPen(QPen(color, width));
        m_painter.drawLine(QPointF(0, y), QPointF(m_width, y));
    }

    // A wrapped paragraph that may run across pages, line by line.
    void paragraph(const QString &text, const QFont &font, qreal indent = 0, const QColor &color = Ink) {
        m_painter.setPen(color);
        const QStringList paragraphs = text.isEmpty() ? QStringList{QString()} : text.split('\n');
        for (const QString &part : paragraphs) {
            QTextLayout layout(part, font, &m_writer);
            QTextOption option;
            option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            layout.setTextOption(option);
            layout.beginLayout();
            QList<QTextLine> lines;
            for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
                line.setLineWidth(m_width - indent);
                line.setPosition(QPointF(0, 0));
                lines.append(line);
            }
            layout.endLayout();
            for (const QTextLine &line : lines) {
                ensure(line.height());
                line.draw(&m_painter, QPointF(indent, m_y));
                m_y += line.height();
            }
        }
    }

    void heading(const QString &text) {
        const QFont font = this->font(10, true);
        ensure(34);
        m_y += 10;
        this->text(QRectF(0, m_y, m_width, 16), text, font);
        m_y += 18;
        rule(m_y - 2);
        m_y += 4;
    }

private:
    void startPage() {
        ++m_page;
        m_y = 0;
        const QString project = m_project.number.isEmpty() ? m_project.name
                                                           : m_project.number + " · " + m_project.name;
        text(QRectF(0, 0, m_width * 0.7, 20), project, font(15, true));
        text(QRectF(0, 0, m_width, 20), m_title, font(11, true), Ink, Qt::AlignRight | Qt::AlignVCenter);
        text(QRectF(0, 20, m_width, 14), tr("Printed %1").arg(date(m_today)), font(8), Muted,
             Qt::AlignRight | Qt::AlignVCenter);
        rule(38, Ink, 1.2);
        m_y = 48;

        const qreal footer = m_writer.height() - 12;
        text(QRectF(0, footer, m_width, 12), "Joist", font(7), Muted);
        text(QRectF(0, footer, m_width, 12), tr("Page %1").arg(m_page), font(7), Muted,
             Qt::AlignRight | Qt::AlignVCenter);
    }

    QPdfWriter m_writer;
    QPainter m_painter;
    const Project &m_project;
    QString m_title;
    QDate m_today;
    qreal m_width = 0;
    qreal m_bottom = 0;
    qreal m_y = 0;
    int m_page = 0;
};

struct Column {
    QString title;
    qreal width; // Fraction of the page.
    std::function<QString(const Item &)> value;
    bool wraps = false;
};

// A table that repeats its header row on every page it runs onto.
void table(Document &document, const QList<Column> &columns, const QList<const Item *> &items, const QDate &today) {
    const QFont headerFont = document.font(7.5, true);
    const QFont cellFont = document.font(8.5);
    const QFontMetricsF metrics = document.metrics(cellFont);
    const qreal pad = 4;

    auto header = [&] {
        document.painter().fillRect(QRectF(0, document.y(), document.width(), 16), Shade);
        qreal x = 0;
        for (const Column &column : columns) {
            const qreal width = column.width * document.width();
            document.text(QRectF(x + pad, document.y(), width - 2 * pad, 16), column.title.toUpper(), headerFont, Muted);
            x += width;
        }
        document.y() += 16;
    };

    document.ensure(40);
    header();
    for (const Item *item : items) {
        qreal height = 16;
        for (const Column &column : columns)
            if (column.wraps) {
                const QRectF bounds = metrics.boundingRect(QRectF(0, 0, column.width * document.width() - 2 * pad, 1e6),
                                                           Qt::TextWordWrap, column.value(*item));
                height = qMax(height, bounds.height() + 6);
            }
        if (document.ensure(height))
            header();
        qreal x = 0;
        for (const Column &column : columns) {
            const qreal width = column.width * document.width();
            const QString value = column.value(*item);
            // The status is always the last column, and goes red when late.
            const bool late = item->isOverdue(today) && &column == &columns.last();
            document.text(QRectF(x + pad, document.y() + 3, width - 2 * pad, height - 6), value,
                          late ? document.font(8.5, true) : cellFont, late ? Late : Ink,
                          Qt::AlignLeft | Qt::AlignTop | (column.wraps ? Qt::TextWordWrap : 0));
            x += width;
        }
        document.y() += height;
        document.rule(document.y());
    }
}

QList<const Item *> sorted(const Project &project, const std::function<bool(const Item &)> &keep) {
    QList<const Item *> items;
    for (const Item &item : project.items)
        if (keep(item))
            items.append(&item);
    std::stable_sort(items.begin(), items.end(), [](const Item *a, const Item *b) {
        return a->type != b->type ? a->type < b->type : a->number < b->number;
    });
    return items;
}

QString logTitle(const QString &kind) {
    if (kind == "rfi")
        return tr("RFI log");
    if (kind == "submittal")
        return tr("Submittal log");
    return tr("Ball in court");
}

QString counts(const QList<const Item *> &items, const QDate &today) {
    const auto open = std::count_if(items.begin(), items.end(), [](const Item *item) { return item->isOpen(); });
    const auto late = std::count_if(items.begin(), items.end(), [&](const Item *item) { return item->isOverdue(today); });
    return tr("%n items", int(items.size())) + " · " + tr("%n open", int(open)) + " · " + tr("%n overdue", int(late));
}

QString dueText(const Item &item, const QDate &today) {
    if (!item.due.isValid())
        return {};
    const qint64 days = today.daysTo(item.due);
    if (days < 0)
        return tr("%n days overdue", int(-days));
    if (days == 0)
        return tr("Due today");
    return tr("Due in %n days", int(days));
}

template <typename Write>
bool writeFile(const QString &path, QString *error, Write write) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = tr("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    if (!write(&file) || !file.commit()) {
        if (error)
            *error = tr("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

} // namespace

bool PdfExport::writeLog(const Project &project, const QString &kind, const QDate &today, const QString &path,
                         QString *error) {
    return writeFile(path, error, [&](QIODevice *device) {
        Document document(device, QPageLayout::Landscape, project, logTitle(kind), today);
        const auto code = [](const Item &item) { return item.code(); };
        const auto title = [](const Item &item) { return item.title; };
        const auto spec = [](const Item &item) { return item.spec; };
        const auto holder = [](const Item &item) { return item.isOpen() ? item.ballInCourt : QString(); };
        const auto created = [](const Item &item) { return date(item.created); };
        const auto due = [](const Item &item) { return item.isOpen() ? date(item.due) : QString(); };
        const auto closed = [](const Item &item) { return date(item.closed); };
        const auto status = [&](const Item &item) { return Labels::status(item, today); };

        if (kind == "board") {
            const auto items = sorted(project, [](const Item &item) { return item.isOpen(); });
            document.text(QRectF(0, document.y(), document.width(), 14), counts(items, today), document.font(9), Muted);
            document.y() += 16;
            // Parties in the project's order, then anyone else still holding something.
            QStringList holders = project.parties;
            for (const Item *item : items)
                if (!holders.contains(item->ballInCourt))
                    holders.append(item->ballInCourt);
            const QList<Column> columns{
                {tr("No."), 0.09, code},
                {tr("Type"), 0.09, [](const Item &item) { return Labels::type(item.type); }},
                {tr("Subject"), 0.40, title, true},
                {tr("Spec"), 0.10, spec},
                {tr("Due"), 0.12, due},
                {tr("Status"), 0.20, [&](const Item &item) { return dueText(item, today); }},
            };
            for (const QString &party : holders) {
                QList<const Item *> held;
                for (const Item *item : items)
                    if (item->ballInCourt == party)
                        held.append(item);
                if (held.isEmpty())
                    continue;
                std::stable_sort(held.begin(), held.end(), [](const Item *a, const Item *b) { return a->due < b->due; });
                document.heading(party.isEmpty() ? tr("Nobody") : party);
                table(document, columns, held, today);
            }
            if (items.isEmpty())
                document.paragraph(tr("Nothing is open."), document.font(10));
            return document.finish();
        }

        const auto items = sorted(project, [&](const Item &item) { return item.type == kind; });
        document.text(QRectF(0, document.y(), document.width(), 14), counts(items, today), document.font(9), Muted);
        document.y() += 20;
        QList<Column> columns;
        if (kind == "rfi")
            columns = {{tr("No."), 0.08, code},           {tr("Subject"), 0.33, title, true},
                       {tr("Spec"), 0.09, spec},          {tr("Ball in court"), 0.13, holder},
                       {tr("Created"), 0.09, created},    {tr("Due"), 0.09, due},
                       {tr("Closed"), 0.09, closed},      {tr("Status"), 0.10, status}};
        else
            columns = {{tr("No."), 0.08, code},
                       {tr("Rev."), 0.04, [](const Item &item) { return QString::number(item.revision); }},
                       {tr("Spec"), 0.09, spec},
                       {tr("Subject"), 0.27, title, true},
                       {tr("Ball in court"), 0.12, holder},
                       {tr("Submitted"), 0.09, created},
                       {tr("Due"), 0.09, due},
                       {tr("Closed"), 0.09, closed},
                       {tr("Status"), 0.13, status}};
        table(document, columns, items, today);
        if (items.isEmpty())
            document.paragraph(tr("Nothing logged yet."), document.font(10));
        return document.finish();
    });
}

bool PdfExport::writeForm(const Project &project, const Item &item, const QDate &today, const QString &path,
                          QString *error) {
    return writeFile(path, error, [&](QIODevice *device) {
        const bool rfi = item.type == "rfi";
        const QString title = rfi ? tr("Request for information") : tr("Submittal");
        Document document(device, QPageLayout::Portrait, project, title, today);

        // The number large, then the facts in two columns.
        const QString number = rfi || item.revision == 0 ? item.code()
                                                         : tr("%1, revision %2").arg(item.code()).arg(item.revision);
        document.text(QRectF(0, document.y(), document.width(), 26), number, document.font(18, true));
        document.y() += 30;

        QString to = item.ballInCourt;
        if (to.isEmpty() && !item.history.isEmpty())
            to = item.history.first().to;
        const QList<QPair<QString, QString>> facts{
            {tr("From"), item.originator()},
            {tr("To"), to},
            {rfi ? tr("Date") : tr("Submitted"), date(item.created)},
            {tr("Spec section"), item.spec},
            {tr("Due"), item.isOpen() ? date(item.due) : QString()},
            {tr("Status"), Labels::status(item, today)},
            {tr("Ball in court"), item.isOpen() ? item.ballInCourt : QString()},
            {tr("Closed"), date(item.closed)},
        };
        const qreal half = document.width() / 2;
        for (int i = 0; i < facts.size(); i += 2) {
            for (int column = 0; column < 2 && i + column < facts.size(); ++column) {
                const qreal x = column * half;
                document.text(QRectF(x, document.y(), 90, 15), facts[i + column].first, document.font(8), Muted);
                document.text(QRectF(x + 92, document.y(), half - 100, 15), facts[i + column].second,
                              document.font(9.5));
            }
            document.y() += 17;
        }

        document.heading(tr("Subject"));
        document.paragraph(item.title, document.font(11, true));
        document.heading(rfi ? tr("Question") : tr("Description"));
        document.paragraph(item.question, document.font(10));
        document.heading(rfi ? tr("Answer") : tr("Review comments"));
        document.paragraph(item.answer, document.font(10));
        // Room to write by hand when there's no answer yet.
        if (item.answer.trimmed().isEmpty() && item.isOpen()) {
            for (int line = 0; line < 5; ++line) {
                document.ensure(22);
                document.y() += 22;
                document.rule(document.y(), Rule, 0.4);
            }
        }

        if (!item.sheets.isEmpty()) {
            document.heading(tr("Drawing references"));
            for (const SheetLink &link : item.sheets)
                document.paragraph(tr("%1 in %2").arg(link.sheet, QFileInfo(link.file).fileName()), document.font(9.5));
        }

        if (!item.history.isEmpty()) {
            document.heading(tr("History"));
            for (const Event &event : item.history) {
                QString line = tr("%1  %2 to %3").arg(date(event.date), event.from, event.to);
                if (!event.note.isEmpty())
                    line += " — " + event.note;
                document.paragraph(line, document.font(9));
            }
        }

        // Signature lines for whoever answers on paper.
        document.ensure(60);
        document.y() += 36;
        const QFont small = document.font(8);
        document.painter().setPen(QPen(Ink, 0.6));
        document.painter().drawLine(QPointF(0, document.y()), QPointF(document.width() * 0.55, document.y()));
        document.painter().drawLine(QPointF(document.width() * 0.65, document.y()), QPointF(document.width(), document.y()));
        document.text(QRectF(0, document.y() + 2, 200, 12), rfi ? tr("Answered by") : tr("Reviewed by"), small, Muted);
        document.text(QRectF(document.width() * 0.65, document.y() + 2, 200, 12), tr("Date"), small, Muted);
        return document.finish();
    });
}

QString PdfExport::suggestedName(const Project &project, const QString &kind, const QDate &today) {
    const QString prefix = project.number.isEmpty() ? project.name : project.number;
    return QString("%1 %2 %3.pdf").arg(prefix, logTitle(kind), today.toString(Qt::ISODate));
}

QString PdfExport::suggestedName(const Project &project, const Item &item) {
    const QString prefix = project.number.isEmpty() ? project.name : project.number;
    QString name = QString("%1 %2").arg(prefix, item.code());
    if (item.type == "submittal" && item.revision > 0)
        name += QString(".%1").arg(item.revision);
    return name + ".pdf";
}
