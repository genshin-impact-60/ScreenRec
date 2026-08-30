#include "floatingbar.h"
#include "appstyle.h"
#include "captureexclude.h"

#include <QEasingCurve>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QShowEvent>
#include <QVariantAnimation>

class RecDot : public QWidget
{
public:
    enum class State { Recording, Paused, Countdown };

    explicit RecDot(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(16, 16);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        m_anim = new QVariantAnimation(this);
        m_anim->setStartValue(0.35);
        m_anim->setEndValue(1.0);
        m_anim->setDuration(900);
        m_anim->setLoopCount(-1);
        m_anim->setEasingCurve(QEasingCurve::InOutSine);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_pulse = value.toReal();
            update();
        });
    }

    void setState(State state)
    {
        m_state = state;
        if (state == State::Recording)
            m_anim->start();
        else {
            m_anim->stop();
            m_pulse = 1.0;
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);

        if (m_state == State::Paused) {
            p.setBrush(QColor(QStringLiteral("#F5C542")));
            const qreal w = 3.2;
            const qreal gap = 2.4;
            const qreal x1 = width() / 2.0 - gap / 2.0 - w;
            const qreal x2 = width() / 2.0 + gap / 2.0;
            p.drawRoundedRect(QRectF(x1, 3, w, height() - 6), 1.2, 1.2);
            p.drawRoundedRect(QRectF(x2, 3, w, height() - 6), 1.2, 1.2);
            return;
        }

        QColor color(QStringLiteral("#E11D2E"));
        if (m_state == State::Recording)
            color.setAlphaF(0.45 + 0.55 * m_pulse);
        p.setBrush(color);
        const qreal r = 5.2 * (m_state == State::Recording ? (0.86 + 0.14 * m_pulse) : 1.0);
        p.drawEllipse(QRectF(width() / 2.0 - r, height() / 2.0 - r, r * 2, r * 2));
    }

private:
    State m_state = State::Recording;
    QVariantAnimation *m_anim = nullptr;
    qreal m_pulse = 1.0;
};

class Grip : public QWidget
{
public:
    explicit Grip(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(10, 22);
        setCursor(Qt::SizeAllCursor);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 90));
        for (int col = 0; col < 2; ++col) {
            for (int row = 0; row < 3; ++row) {
                p.drawEllipse(QPointF(3 + col * 4, 5 + row * 6), 1.4, 1.4);
            }
        }
    }
};

FloatingBar::FloatingBar(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedHeight(48);

    auto *inner = new QWidget(this);
    inner->setObjectName(QStringLiteral("barInner"));
    inner->setAttribute(Qt::WA_StyledBackground, true);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(inner);

    auto *root = new QHBoxLayout(inner);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(10);

    auto *grip = new Grip(inner);
    m_dot = new RecDot(inner);
    m_duration = new QLabel(QStringLiteral("00:00"), inner);
    m_duration->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont font = m_duration->font();
    font.setFamilies({QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas"),
                      QStringLiteral("Segoe UI")});
    font.setBold(true);
    font.setPixelSize(14);
    m_duration->setFont(font);

    m_pauseButton = new QPushButton(tr("暂停"), inner);
    m_stopButton = new QPushButton(tr("停止"), inner);
    m_pauseButton->setObjectName(QStringLiteral("pauseBtn"));
    m_stopButton->setObjectName(QStringLiteral("stopBtn"));
    m_pauseButton->setCursor(Qt::PointingHandCursor);
    m_stopButton->setCursor(Qt::PointingHandCursor);
    m_pauseButton->setIcon(AppIcons::pause());
    m_stopButton->setIcon(AppIcons::stop());
    m_pauseButton->setIconSize(QSize(12, 12));
    m_stopButton->setIconSize(QSize(12, 12));

    root->addWidget(grip);
    root->addWidget(m_dot);
    root->addWidget(m_duration);
    root->addSpacing(6);
    root->addWidget(m_pauseButton);
    root->addWidget(m_stopButton);

    inner->setStyleSheet(QStringLiteral(
        "#barInner { background: rgba(22, 24, 28, 230); border-radius: 12px; }"
        "QLabel { color: white; background: transparent; }"
        "QPushButton { color: white; border: none; padding: 4px 12px;"
        " border-radius: 8px; min-height: 28px; font-weight: 600; }"
        "QPushButton#pauseBtn { background: #3A3D44; }"
        "QPushButton#pauseBtn:hover { background: #4A4E56; }"
        "QPushButton#stopBtn { background: #E11D2E; }"
        "QPushButton#stopBtn:hover { background: #C91828; }"));

    inner->setCursor(Qt::SizeAllCursor);
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
    if (countdown)
        m_dot->setState(RecDot::State::Countdown);
    else if (paused)
        m_dot->setState(RecDot::State::Paused);
    else
        m_dot->setState(RecDot::State::Recording);

    m_pauseButton->setVisible(!countdown);
    m_pauseButton->setText(paused ? tr("继续") : tr("暂停"));
    m_pauseButton->setIcon(paused ? AppIcons::resume() : AppIcons::pause());
    m_stopButton->setText(countdown ? tr("取消") : tr("停止"));
    adjustSize();
}

void FloatingBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}

void FloatingBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton))
        move(event->globalPosition().toPoint() - m_dragOffset);
}

void FloatingBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        emit positionChanged(pos());
    }
}
