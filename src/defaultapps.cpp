#include "defaultapps.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include "i18n.h"

namespace {
const char* kApp = "Vidline";
const char* kDesktopId = "vidline.desktop";
const char* kBundleId = "io.github.cutline";
const QStringList kWApps = {"Vidline", "Cutline", "Clipline", "Webline", "Webcase", "Textline", "Picline", "Convertline"};

QString ownerOf(const QString& appName) {
    for (const QString& w : kWApps)
        if (appName.contains(w, Qt::CaseInsensitive)) return w == "Cutline" ? QString(kApp) : w;
    return {};
}

QString run(const QString& program, const QStringList& args, int ms = 20000) {
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(ms)) {
        p.kill();
        return {};
    }
    return QString::fromUtf8(p.readAllStandardOutput());
}

#if defined(Q_OS_WIN)
// Benutzerwahl (UserChoice) zuerst, sonst die Endung; "App auswählen" (OpenWith.exe) = noch kein Standard
QString windowsScript(const QStringList& exts) {
    QStringList quoted;
    for (const QString& e : exts) quoted << "'" + e + "'";
    return QString(R"PS([Console]::OutputEncoding = [Text.Encoding]::UTF8
Add-Type -Namespace WHub -Name Assoc -MemberDefinition '[DllImport("Shlwapi.dll", CharSet = CharSet.Unicode)] public static extern uint AssocQueryString(uint flags, uint str, string assoc, string extra, System.Text.StringBuilder outStr, ref uint len);'
$res = @{}
foreach ($e in @(%1)) {
  $uc = Get-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.$e\UserChoice" -ErrorAction SilentlyContinue
  $q = if ($uc.ProgId) { [string]$uc.ProgId } else { ".$e" }
  $sb = New-Object System.Text.StringBuilder 512
  [uint32]$len = 512
  $null = [WHub.Assoc]::AssocQueryString(0, 4, $q, 'open', $sb, [ref]$len)
  $exe = New-Object System.Text.StringBuilder 1024
  [uint32]$len = 1024
  $null = [WHub.Assoc]::AssocQueryString(0, 2, $q, 'open', $exe, [ref]$len)
  if ($exe.ToString() -match 'OpenWith\.exe$') { $sb.Clear() | Out-Null }
  $res[$e] = @{ progId = [string]$uc.ProgId; app = $sb.ToString() }
}
$where = ''
if (Get-ItemProperty 'HKCU:\Software\RegisteredApplications' -Name 'Vidline' -ErrorAction SilentlyContinue) { $where = 'user' }
elseif (Get-ItemProperty 'HKLM:\Software\RegisteredApplications' -Name 'Vidline' -ErrorAction SilentlyContinue) { $where = 'machine' }
ConvertTo-Json -Compress -Depth 4 @{ types = $res; registered = $where })PS")
        .arg(quoted.join(","));
}
QString encoded(const QString& script) {
    const QByteArray utf16(reinterpret_cast<const char*>(script.utf16()), script.size() * 2);
    return QString::fromLatin1(utf16.toBase64());
}
#endif

#if defined(Q_OS_LINUX)
QString desktopName(const QString& id) {
    QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString& d : dirs) {
        QFile f(QDir(d).filePath(id));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QString text = QString::fromUtf8(f.readAll());
        const auto m = QRegularExpression("^Name=(.+)$", QRegularExpression::MultilineOption).match(text);
        if (m.hasMatch()) return m.captured(1).trimmed();
    }
    return QString(id).remove(".desktop");
}
#endif

#if defined(Q_OS_MACOS)
const char* kMacState = R"JS(ObjC.import('AppKit'); ObjC.import('UniformTypeIdentifiers');
function run(argv) { const utis = JSON.parse(argv[0]); const ws = $.NSWorkspace.sharedWorkspace; const out = {};
  for (const u of utis) { try { const t = $.UTType.typeWithIdentifier(u); const url = t ? ws.URLForApplicationToOpenContentType(t) : null;
    if (!url || url.isNil()) { out[u] = null; continue; } const b = $.NSBundle.bundleWithURL(url);
    out[u] = { name: ObjC.unwrap(url.lastPathComponent).replace(/\.app$/, ''), id: b && !b.isNil() ? ObjC.unwrap(b.bundleIdentifier) : '' }; } catch (e) { out[u] = null; } }
  return JSON.stringify(out); })JS";
const char* kMacSet = R"JS(ObjC.import('CoreServices');
function run(argv) { const utis = JSON.parse(argv[0]); const id = argv[1]; const bad = [];
  for (const u of utis) { const r = $.LSSetDefaultRoleHandlerForContentType($(u), 0xFFFFFFFF, $(id)); if (r !== 0) bad.push(u + ':' + r); }
  return JSON.stringify(bad); })JS";
#endif
}  // namespace

DefaultAppsDialog::DefaultAppsDialog(QWidget* parent) : QDialog(parent) {
    groups_ = {
        {"video", "da_video", {"mp4", "mov", "mkv", "m4v", "avi", "webm", "wmv", "flv"},
         {"video/mp4", "video/quicktime", "video/x-matroska", "video/webm", "video/x-msvideo"},
         {"public.mpeg-4", "com.apple.quicktime-movie", "org.matroska.mkv", "public.avi", "org.webmproject.webm"}},
        {"music", "da_music", {"mp3", "wav", "m4a", "aac", "flac", "ogg", "opus", "wma"},
         {"audio/mpeg", "audio/x-wav", "audio/mp4", "audio/aac", "audio/flac", "audio/ogg", "audio/opus"},
         {"public.mp3", "com.microsoft.waveform-audio", "public.mpeg-4-audio", "public.aac-audio", "org.xiph.flac"}},
    };
    for (int i = 0; i < groups_.size(); ++i) checked_ << -1;
    setWindowTitle(T("da_title"));
    setMinimumWidth(560);
    auto* v = new QVBoxLayout(this);
    auto* intro = new QLabel(T("da_intro").arg(kApp));
    intro->setWordWrap(true);
    v->addWidget(intro);
#if defined(Q_OS_WIN)
    auto* how = new QLabel(T("da_winHow").arg(kApp));
    how->setWordWrap(true);
    how->setObjectName("hint");
    v->addWidget(how);
#endif
    auto* list = new QFrame;
    list->setObjectName("card");
    auto* grid = new QGridLayout(list);
    grid->setHorizontalSpacing(12);
    for (int i = 0; i < groups_.size(); ++i) {
        const Group& g = groups_[i];
        auto* box = new QCheckBox;
        auto* name = new QLabel(T(g.labelKey.toUtf8().constData()));
        name->setObjectName("title");
        QStringList hint;
        for (const QString& e : g.exts) hint << "." + e;
        auto* exts = new QLabel(hint.join(" "));
        exts->setObjectName("hint");
        auto* now = new QLabel(T("da_loading"));
        now->setObjectName("hint");
        auto* names = new QVBoxLayout;
        names->setSpacing(0);
        names->addWidget(name);
        names->addWidget(exts);
        grid->addWidget(box, i, 0, Qt::AlignTop);
        grid->addLayout(names, i, 1);
        grid->addWidget(now, i, 2, Qt::AlignRight | Qt::AlignVCenter);
        connect(box, &QCheckBox::toggled, this, [this, i](bool on) {
            checked_[i] = on ? 1 : 0;
            render();
        });
        connect(name, &QLabel::linkActivated, box, &QCheckBox::toggle);
        boxes_ << box;
        nows_ << now;
    }
    grid->setColumnStretch(1, 1);
    v->addWidget(list);
    note_ = new QLabel;
    note_->setWordWrap(true);
    note_->setObjectName("hint");
    note_->hide();
    v->addWidget(note_);
    auto* row = new QHBoxLayout;
    row->addStretch();
    close_ = new QPushButton(T("da_cancel"));
    connect(close_, &QPushButton::clicked, this, &QDialog::reject);
    apply_ = new QPushButton(
#if defined(Q_OS_WIN)
        T("da_openSettings")
#else
        T("da_apply")
#endif
    );
    apply_->setObjectName("primary");
    apply_->setEnabled(false);
    connect(apply_, &QPushButton::clicked, this, &DefaultAppsDialog::apply);
    row->addWidget(close_);
    row->addWidget(apply_);
    v->addLayout(row);
    poll_ = new QTimer(this);
    poll_->setInterval(2500);
    connect(poll_, &QTimer::timeout, this, &DefaultAppsDialog::refresh);
    QTimer::singleShot(0, this, &DefaultAppsDialog::refresh);
}

QList<DefaultAppsDialog::State> DefaultAppsDialog::readState(QString* registered) {
    QList<State> out;
#if defined(Q_OS_WIN)
    QStringList exts;
    for (const Group& g : groups_) exts << g.exts;
    const QString json = run("powershell.exe", {"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand",
                                                encoded(windowsScript(exts))});
    const QJsonObject root = QJsonDocument::fromJson(json.trimmed().toUtf8()).object();
    const QJsonObject types = root.value("types").toObject();
    *registered = root.value("registered").toString();
    for (const Group& g : groups_) {
        State s;
        s.total = g.exts.size();
        for (const QString& e : g.exts) {
            const QJsonObject t = types.value(e).toObject();
            const QString prog = t.value("progId").toString(), app = t.value("app").toString();
            const bool mine = prog.startsWith("Cutline.", Qt::CaseInsensitive) || prog.startsWith("Vidline.", Qt::CaseInsensitive) ||
                              ownerOf(app) == kApp;
            if (mine) ++s.done;
            else if (s.current.isEmpty() && !app.isEmpty()) s.current = app;
        }
        s.mine = s.done == s.total;
        s.owner = ownerOf(s.current);
        out << s;
    }
#elif defined(Q_OS_MACOS)
    *registered = "user";
    QStringList utis;
    for (const Group& g : groups_) utis << g.utis.first();
    const QString json = run("osascript", {"-l", "JavaScript", "-e", kMacState,
                                           QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(utis)).toJson(QJsonDocument::Compact))});
    const QJsonObject root = QJsonDocument::fromJson(json.trimmed().toUtf8()).object();
    for (const Group& g : groups_) {
        State s;
        const QJsonObject cur = root.value(g.utis.first()).toObject();
        s.mine = cur.value("id").toString() == kBundleId;
        s.done = s.mine ? 1 : 0;
        if (!s.mine) s.current = cur.value("name").toString();
        s.owner = ownerOf(s.current);
        out << s;
    }
#else
    *registered = "user";
    for (const Group& g : groups_) {
        State s;
        const QString id = run("xdg-mime", {"query", "default", g.mimes.first()}).trimmed();
        s.mine = id == kDesktopId;
        s.done = s.mine ? 1 : 0;
        if (!s.mine && !id.isEmpty()) s.current = desktopName(id);
        s.owner = ownerOf(s.current);
        out << s;
    }
#endif
    return out;
}

void DefaultAppsDialog::refresh() {
    setCursor(Qt::BusyCursor);
    state_ = readState(&registered_);
    unsetCursor();
    render();
}

void DefaultAppsDialog::render() {
    bool any = false;
    for (int i = 0; i < groups_.size(); ++i) {
        const State s = i < state_.size() ? state_[i] : State();
        if (checked_[i] < 0 && i < state_.size()) checked_[i] = (!s.mine && (s.owner.isEmpty() || s.owner == kApp)) ? 1 : 0;
        QSignalBlocker block(boxes_[i]);
        boxes_[i]->setChecked(s.mine || checked_[i] == 1);
        boxes_[i]->setEnabled(!s.mine);
        QString text;
        if (s.mine) text = T("da_mine").arg(kApp);
        else if (s.total > 1 && s.done) text = T("da_part").arg(s.done).arg(s.total);
        else text = s.current.isEmpty() ? T("da_nobody") : T("da_now").arg(s.current);
        nows_[i]->setText(text);
        if (!s.mine && checked_[i] == 1) any = true;
    }
    apply_->setEnabled(any);
}

void DefaultAppsDialog::apply() {
    QStringList mimes, utis;
    for (int i = 0; i < groups_.size(); ++i) {
        if (checked_[i] != 1 || (i < state_.size() && state_[i].mine)) continue;
        mimes << groups_[i].mimes;
        utis << groups_[i].utis;
    }
    if (mimes.isEmpty()) return;
    apply_->setEnabled(false);
#if defined(Q_OS_WIN)
    if (registered_.isEmpty()) {
        note_->setText(T("da_notRegistered").arg(kApp));
        note_->show();
    }
    const QString key = registered_ == "machine" ? "registeredAppMachine" : "registeredAppUser";
    QDesktopServices::openUrl(QUrl(registered_.isEmpty() ? QString("ms-settings:defaultapps")
                                                         : QString("ms-settings:defaultapps?%1=%2").arg(key, kApp)));
    close_->setText(T("da_close"));
    poll_->start();
#else
    QString error;
#if defined(Q_OS_MACOS)
    const QString res = run("osascript", {"-l", "JavaScript", "-e", kMacSet,
                                          QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(utis)).toJson(QJsonDocument::Compact)), kBundleId});
    const QJsonArray bad = QJsonDocument::fromJson(res.trimmed().toUtf8()).array();
    if (res.trimmed().isEmpty() || !bad.isEmpty()) error = res.trimmed().isEmpty() ? QString("osascript") : bad.first().toString();
#else
    QProcess p;
    p.start("xdg-mime", QStringList{"default", kDesktopId} + mimes);
    if (!p.waitForFinished(15000) || p.exitCode() != 0) error = QString::fromUtf8(p.readAllStandardError()).trimmed().left(200);
#endif
    refresh();
    int n = 0;
    for (const State& s : state_) n += s.mine ? 1 : 0;
    if (error.isEmpty()) note_->setText(T("da_done").arg(kApp).arg(n));
    else
        note_->setText(T("da_failed").arg(error)
#if defined(Q_OS_MACOS)
                       + " " + T("da_macHand").arg(kApp)
#endif
        );
    note_->show();
    close_->setText(T("da_close"));
#endif
}
