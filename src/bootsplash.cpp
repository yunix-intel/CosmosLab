// ============================================================================
//  bootsplash.cpp —— QRasterWindow 手绘闪屏实现
// ============================================================================

#include "bootsplash.h"

#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QLinearGradient>
#include <QScreen>
#include <QGuiApplication>

BootSplash::BootSplash(const QPixmap &bg, bool headless, QRasterWindow *parent)
    : QRasterWindow(parent)
    , m_bg(bg)
{
    // 居中 900x560, 与主窗口同风格的深空底
    setFlags(Qt::SplashScreen | Qt::FramelessWindowHint);
    resize(900, 560);
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        const QRect g = scr->availableGeometry();
        setPosition((g.width() - 900) / 2 + g.x(),
                    (g.height() - 560) / 2 + g.y());
    }
    // 全局字体由 main.cpp 统一设置, 这里不再重复。
    if (!headless)
        show();
}

void BootSplash::setStage(const QString &text)
{
    m_stage = text;
    if (isVisible())
        update();
}

void BootSplash::finish()
{
    if (!isVisible()) {
        close();
        return;
    }
    // 8 帧淡出 (~130ms), 避免闪白
    m_fadeTimer = new QTimer(this);
    connect(m_fadeTimer, &QTimer::timeout, this, [this] {
        m_opacity -= 0.125;
        if (m_opacity <= 0.0) {
            m_fadeTimer->stop();
            close();
            return;
        }
        setOpacity(m_opacity);
        update();
    });
    m_fadeTimer->start(16);
}

void BootSplash::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int W = width(), H = height();

    // ---- 背景: 有图铺图 (居中裁剪), 无图纯色 ----
    if (!m_bg.isNull()) {
        const QPixmap scaled = m_bg.scaled(
            QSize(W, H), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int x = (scaled.width() - W) / 2;
        const int y = (scaled.height() - H) / 2;
        p.drawPixmap(0, 0, W, H, scaled, x, y, W, H);
    } else {
        p.fillRect(0, 0, W, H, QColor(0x05, 0x07, 0x0d));
    }

    // ---- 左侧渐隐罩 (标题可读) ----
    QLinearGradient lg(0, 0, W, 0);
    lg.setColorAt(0.00, QColor(1, 2, 5, 210));
    lg.setColorAt(0.45, QColor(1, 2, 5, 115));
    lg.setColorAt(0.75, QColor(1, 2, 5, 12));
    lg.setColorAt(1.00, QColor(1, 2, 5, 0));
    p.fillRect(0, 0, W, H, lg);

    // ---- 底部渐隐罩 (阶段文案可读) ----
    QLinearGradient bg2(0, H * 0.62, 0, H);
    bg2.setColorAt(0.0, QColor(1, 2, 5, 0));
    bg2.setColorAt(1.0, QColor(1, 2, 5, 200));
    p.fillRect(0, 0, W, H, bg2);

    // ---- 标题 ----
    QFont t(QGuiApplication::font());
    t.setPixelSize(44);
    t.setBold(true);
    t.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    p.setFont(t);
    p.setPen(QColor(0xf2, 0xf6, 0xff));
    p.drawText(72, 120, 460, 60, Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("宇宙实验室"));

    QFont s(t);
    s.setPixelSize(13);
    s.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    p.setFont(s);
    p.setPen(QColor(158, 189, 235, 230));
    p.drawText(72, 172, 460, 24, Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("COSMOSLAB · 从太阳系到可观测宇宙"));
    p.fillRect(78, 206, 64, 2, QColor(0x5e, 0xa9, 0xff));

    // ---- 阶段文案 + 版本 ----
    QFont st(t);
    st.setPixelSize(13);
    p.setFont(st);
    p.setPen(QColor(0xdb, 0xe6, 0xf7));
    p.drawText(72, H - 78, 380, 24, Qt::AlignLeft | Qt::AlignVCenter, m_stage);

    QFont v(t);
    v.setPixelSize(10);
    v.setFamily(QStringLiteral("Consolas"));
    p.setFont(v);
    p.setPen(QColor(140, 161, 191, 180));
    p.drawText(72, H - 50, 380, 20, Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("v2.0 · C++ / QML / OpenGL"));
}
