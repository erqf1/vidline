#include "plugins.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include "i18n.h"
#include "theme.h"

const QList<PluginInfo>& plugins() {
    static const QList<PluginInfo> list = {
        {"videodl", "plugin_videodl", "plugin_videodl_desc", Ic::Download},
        {"subtitles", "plugin_subs", "plugin_subs_desc", Ic::Subtitles},
    };
    return list;
}

bool pluginEnabled(const char* id) { return QSettings().value(QString("plugins/") + id, false).toBool(); }

void setPluginEnabled(const char* id, bool on) { QSettings().setValue(QString("plugins/") + id, on); }

QString pluginDataDir() {
    QString d = qEnvironmentVariable("VIDLINE_DATA");
    if (d.isEmpty()) d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return d;
}

QString ytDlpPath() {
#ifdef Q_OS_WIN
    return pluginDataDir() + "/plugins/yt-dlp.exe";
#else
    return pluginDataDir() + "/plugins/yt-dlp";
#endif
}

void showTunelineHint(QWidget* parent) {
    QSettings st;
    if (!st.value("plugins/music", false).toBool() || st.value("tuneline/hinted", false).toBool()) return;
    st.setValue("tuneline/hinted", true);
    st.setValue("plugins/music", false);
    QMessageBox box(parent);
    box.setWindowTitle(T("tune_title"));
    box.setIcon(QMessageBox::Information);
    box.setText(T("tune_title"));
    box.setInformativeText(T("tune_text"));
    QPushButton* get = box.addButton(T("tune_get"), QMessageBox::AcceptRole);
    box.addButton(T("ok"), QMessageBox::RejectRole);
    box.setDefaultButton(get);
    box.exec();
    if (box.clickedButton() == get) QDesktopServices::openUrl(QUrl("https://mert.win/tune/"));
}

PluginDialog::PluginDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(T("plugins"));
    setMinimumWidth(460);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(14);
    auto* hint = new QLabel(T("plugins_hint"));
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    v->addWidget(hint);
    list_ = new QVBoxLayout;
    list_->setSpacing(10);
    v->addLayout(list_);
    auto* more = new QLabel(T("plugins_more"));
    more->setObjectName("hint");
    more->setWordWrap(true);
    v->addWidget(more);
    v->addStretch();
    auto* ok = new QPushButton(T("ok"));
    ok->setObjectName("primary");
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    v->addWidget(ok);
    rebuild();
}

void PluginDialog::rebuild() {
    while (QLayoutItem* it = list_->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    const Theme& th = currentTheme();
    for (const PluginInfo& p : plugins()) {
        auto* card = new QFrame;
        card->setObjectName("card");
        auto* h = new QHBoxLayout(card);
        h->setContentsMargins(14, 12, 14, 12);
        h->setSpacing(12);
        auto* icon = new QLabel;
        icon->setPixmap(makeIcon(p.icon, th.accent).pixmap(32, 32));
        icon->setFixedSize(36, 36);
        icon->setAlignment(Qt::AlignCenter);
        h->addWidget(icon, 0, Qt::AlignTop);
        auto* text = new QVBoxLayout;
        text->setSpacing(4);
        const bool on = pluginEnabled(p.id);
        auto* name = new QLabel(T(p.nameKey) + (on ? QString("  ·  ") + T("plugin_installed") : QString()));
        name->setObjectName("title");
        auto* desc = new QLabel(T(p.descKey));
        desc->setObjectName("hint");
        desc->setWordWrap(true);
        text->addWidget(name);
        text->addWidget(desc);
        h->addLayout(text, 1);
        auto* btn = new QPushButton(on ? T("plugin_remove") : T("plugin_install"));
        btn->setObjectName(on ? "" : "primary");
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, id = p.id, on] {
            setPluginEnabled(id, !on);
            emit changed();
            QMetaObject::invokeMethod(this, [this] { rebuild(); }, Qt::QueuedConnection);
        });
        h->addWidget(btn, 0, Qt::AlignVCenter);
        list_->addWidget(card);
    }
}
