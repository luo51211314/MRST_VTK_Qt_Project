#pragma once
#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class VtkViewHost : public QFrame {
    Q_OBJECT
public:
    explicit VtkViewHost(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName("renderHost");
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        layout_ = new QVBoxLayout(this);
        layout_->setContentsMargins(0, 0, 0, 0);
        layout_->setSpacing(0);

        placeholder_ = new QLabel(tr("VTK 渲染区（占位）"), this);
        placeholder_->setAlignment(Qt::AlignCenter);
        placeholder_->setObjectName("renderPlaceholder");
        layout_->addWidget(placeholder_, 1);

        setStyleSheet(R"(
            QFrame#renderHost {
                background: white;
                border: 1px solid #d6d6d6;
                border-radius: 6px;
            }
            QLabel#renderPlaceholder { color:#777; font-size:14px; }
        )");
    }

    void setPlaceholderText(const QString& text) {
        if (placeholder_) placeholder_->setText(text);
    }

    void setRenderWidget(QWidget* w) {
        // 清空
        while (QLayoutItem* item = layout_->takeAt(0)) {
            if (QWidget* ww = item->widget()) ww->deleteLater();
            delete item;
        }
        if (!w) return;
        w->setParent(this);
        w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout_->addWidget(w, 1);
    }

private:
    QVBoxLayout* layout_ = nullptr;
    QLabel* placeholder_ = nullptr;
};
