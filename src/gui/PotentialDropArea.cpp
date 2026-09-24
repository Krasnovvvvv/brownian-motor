#include "gui/PotentialDropArea.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QUrl>
#include <QVBoxLayout>

namespace {

[[nodiscard]] QString local_file_from_mime_data(
    const QMimeData* mime_data
) {
    if (
        !mime_data ||
        !mime_data->hasUrls() ||
        mime_data->urls().size() != 1
    ) {
        return {};
    }

    const QUrl url =
        mime_data->urls().front();

    if (!url.isLocalFile()) {
        return {};
    }

    const QString file_name =
        url.toLocalFile();

    if (
        !file_name.endsWith(
            ".bmpotential",
            Qt::CaseInsensitive
        ) &&
        !file_name.endsWith(
            ".json",
            Qt::CaseInsensitive
        )
    ) {
        return {};
    }

    return file_name;
}

} // namespace

PotentialDropArea::PotentialDropArea(
    QWidget* parent
)
    : QFrame{parent}
{
    setAcceptDrops(true);

    setFrameShape(
        QFrame::StyledPanel
    );

    setMinimumHeight(48);

    auto* layout = new QVBoxLayout{this};

    layout->setContentsMargins(
        6,
        4,
        6,
        4
    );

    message_label_ = new QLabel{
        "Drop a .bmpotential or .json file here",
        this
    };

    message_label_->setAlignment(
        Qt::AlignCenter
    );

    message_label_->setWordWrap(true);

    message_label_->setAttribute(
        Qt::WA_TransparentForMouseEvents
    );

    layout->addWidget(
        message_label_
    );
}

void PotentialDropArea::set_message(
    const QString& message,
    const QString& details
) {
    message_label_->setText(message);

    setToolTip(
        details.isEmpty()
            ? message
            : details
    );
}

void PotentialDropArea::dragEnterEvent(
    QDragEnterEvent* event
) {
    if (
        isEnabled() &&
        !local_file_from_mime_data(
            event->mimeData()
        ).isEmpty()
    ) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void PotentialDropArea::dropEvent(
    QDropEvent* event
) {
    const QString file_name =
        local_file_from_mime_data(
            event->mimeData()
        );

    if (
        !isEnabled() ||
        file_name.isEmpty()
    ) {
        event->ignore();
        return;
    }

    event->acceptProposedAction();

    emit file_dropped(
        file_name
    );
}