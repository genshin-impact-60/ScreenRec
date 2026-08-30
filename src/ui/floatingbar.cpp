#include "floatingbar.h"
#include "captureexclude.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>

FloatingBar::FloatingBar(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(40);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(10, 4, 10, 4);
    root->setSpacing(8);

    m_dot = new QLabel(QStringLiteral("●"), this);
    m_dot->setStyleSheet(QStringLiteral("color: #e74c3c; font-size: 16px;"));
    m_duration = new QLabel(QStringLiteral("00:00:00"), this);
    QFont font = m_duration->font();
    font.setBold(true);
    m_duration->setFont(font);

    m_pauseButton = new QPushButton(tr("暂停"), this);
    m_stopButton = new QPushButton(tr("停止"), this);

    root->addWidget(m_dot);
    root->addWidget(m_duration);
    root->addSpacing(8);
    root->addWidget(m_pauseButton);
    root->addWidget(m_stopButton);

    setStyleSheet(QStringLiteral(
        "FloatingBar { background: rgba(20, 20, 20, 210); border-radius: 8px; }"
        "QLabel { color: white; }"
        "QPushButton { color: white; background: #3d3d3d; border: none; padding: 4px 10px;"
        " border-radius: 4px; }"
        "QPushButton:hover { background: #505050; }"));

    adjustSize();

    connect(m_pauseButton, &QPushButton::clicked, this, &FloatingBar::pauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &FloatingBar::stopClicked);
}

void FloatingBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    excludeWidgetFromCapture(this);
}

void FloatingBar::setDurationText(const QString &text)
{
    m_duration->setText(text);
}

void FloatingBar::setRecordingUi(bool paused, bool countdown)
{
    m_dot->setText(countdown ? QStringLiteral("●") : (paused ? QStringLiteral("❚❚") : QStringLiteral("●")));
    m_pauseButton->setVisible(!countdown);
    m_pauseButton->setText(paused ? tr("继续") : tr("暂停"));
    m_stopButton->setText(countdown ? tr("取消") : tr("停止"));
    adjustSize();
}

void FloatingBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
}

void FloatingBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        move(event->globalPosition().toPoint() - m_dragOffset);
}
