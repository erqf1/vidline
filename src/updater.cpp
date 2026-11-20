#include "updater.h"

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>
#include <memory>

namespace {
QString sq(const QString& s) {  // für sh: '...'
    QString r = s;
    r.replace("'", "'\\''");
    return "'" + r + "'";
}
QString pq(const QString& s) {  // für PowerShell: '...'
    QString r = s;
    r.replace("'", "''");
    return "'" + r + "'";
}
bool dirWritable(const QString& dir) {
    QFile f(dir + "/.update-write-test");
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.close();
    f.remove();
    return true;
}

#if defined(Q_OS_WIN)
// Wie wurde installiert? Inno Setup trägt sich unter HKLM (alle Benutzer) oder HKCU (nur ich) ein.
// Mit dem falschen Modus findet das Setup die alte Installation nicht und installiert woanders hin.
QString installScope(const QString& appId, const QString& appDir) {
    if (!appId.isEmpty()) {
        const QString key = QString("\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{%1}_is1").arg(appId);
        for (auto fmt : {QSettings::Registry64Format, QSettings::Registry32Format})
            if (QSettings("HKEY_LOCAL_MACHINE" + key, fmt).contains("UninstallString")) return "/ALLUSERS";
        if (QSettings("HKEY_CURRENT_USER" + key, QSettings::NativeFormat).contains("UninstallString")) return "/CURRENTUSER";
    }
    return dirWritable(appDir) ? "/CURRENTUSER" : "/ALLUSERS";
}
#endif
}  // namespace

Updater::Updater(Options o, QObject* parent) : QObject(parent), o_(std::move(o)), net_(new QNetworkAccessManager(this)) {
    // Entwickler-Test: ältere Version vortäuschen / Dialog automatisch bestätigen
    if (qEnvironmentVariableIsSet("UPDATER_FAKE_VERSION")) o_.version = qEnvironmentVariable("UPDATER_FAKE_VERSION");
}

void Updater::startAutoCheck(int firstDelayMs) {
    QTimer::singleShot(firstDelayMs, this, [this] { check(false); });
    auto* hourly = new QTimer(this);
    connect(hourly, &QTimer::timeout, this, [this] { check(false); });
    hourly->start(60 * 60 * 1000);
}

void Updater::check(bool manual) {
    if (busy_) return;
    busy_ = true;
    QNetworkRequest req(QUrl(QString("https://api.github.com/repos/%1/releases/latest").arg(o_.repo)));
    req.setHeader(QNetworkRequest::UserAgentHeader, o_.appName + "/" + o_.version);
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(15000);
    QNetworkReply* r = net_->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r, manual] {
        r->deleteLater();
        const QJsonObject rel = QJsonDocument::fromJson(r->readAll()).object();
        if (r->error() != QNetworkReply::NoError || rel.isEmpty()) {
            if (manual) QMessageBox::warning(o_.parent ? o_.parent() : nullptr, o_.appName, r->errorString());
            // Automatische Suche gescheitert (offline, GitHub-Limit): in 10 Minuten nochmal
            else QTimer::singleShot(10 * 60 * 1000, this, [this] { check(false); });
            busy_ = false;
            return;
        }
        onRelease(rel, manual);  // blockiert, solange die Frage offen ist
        busy_ = installing_;     // während des Downloads keine neue Suche
    });
}

void Updater::onRelease(const QJsonObject& rel, bool manual) {
    const UpdaterTexts tx = o_.texts();
    QWidget* parent = o_.parent ? o_.parent() : nullptr;
    QString tag = rel.value("tag_name").toString();
    const QString ver = tag.startsWith('v') ? tag.mid(1) : tag;
    const QString page = rel.value("html_url").toString();
    const bool newer = QVersionNumber::fromString(ver) > QVersionNumber::fromString(o_.version);
    QSettings st;
    // Letzter Versuch, genau diese Version zu installieren, hat nicht geklappt: nicht wieder
    // "Jetzt updaten" anbieten (Endlosschleife), sondern ehrlich sagen und die Download-Seite öffnen
    const QString attempted = st.value("update/attempted").toString();
    if (!attempted.isEmpty()) {
        st.remove("update/attempted");
        if (newer && attempted == tag) return fail(page);
    }
    if (!newer) {
        if (manual) QMessageBox::information(parent, o_.appName, tx.upToDate);
        return;
    }
    // "Dieses Update ignorieren": erst bei der nächsten neuen Version wieder melden
    if (!manual && st.value("update/ignored").toString() == tag) return;
    // "Später": erst wieder fragen, wenn eine noch neuere Version erscheint oder zwei Wochen um sind
    if (!manual && st.value("update/later").toString() == tag &&
        QDateTime::currentDateTime() < st.value("update/laterUntil").toDateTime())
        return;

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tx.title);
    box.setText("<b>" + tx.title + "</b>");
    box.setInformativeText(tx.text.arg(o_.appName, ver, o_.version));
    auto* bNow = box.addButton(tx.now, QMessageBox::AcceptRole);
    auto* bIgnore = box.addButton(tx.ignore, QMessageBox::DestructiveRole);
    box.addButton(tx.later, QMessageBox::RejectRole);
    box.setDefaultButton(bNow);
    if (qEnvironmentVariable("UPDATER_AUTO") == "now") QTimer::singleShot(0, bNow, &QPushButton::click);
    box.exec();
    if (box.clickedButton() == bIgnore) {
        st.setValue("update/ignored", tag);
        return;
    }
    if (box.clickedButton() != bNow) {
        st.setValue("update/later", tag);
        st.setValue("update/laterUntil", QDateTime::currentDateTime().addDays(14));
        return;
    }
    if (o_.beforeInstall && !o_.beforeInstall()) return;

    QString url;
    const QString name = pickAsset(rel, &url);
    if (name.isEmpty()) return fail(page);
    download(url, name, page, tag);
}

// Passendes Paket für dieses System und diese Installationsart
QString Updater::pickAsset(const QJsonObject& rel, QString* url) const {
    const QString exe = QCoreApplication::applicationFilePath();
    const QString dir = QCoreApplication::applicationDirPath();
    QString suffix;
#if defined(Q_OS_WIN)
    suffix = QFile::exists(dir + "/unins000.exe") ? "-windows-x64-setup.exe" : "-windows-x64.zip";
#elif defined(Q_OS_MACOS)
    suffix = QSysInfo::currentCpuArchitecture().contains("arm") ? "-macos-apple-silicon.dmg" : "-macos-intel.dmg";
#else
    if (QSysInfo::currentCpuArchitecture() != "x86_64") return QString();
    if (exe.startsWith("/usr/")) {
        const bool deb = !QStandardPaths::findExecutable("dpkg").isEmpty() &&
                         QProcess::execute("dpkg", {"-S", exe}) == 0;
        if (deb) suffix = "_amd64.deb";
        else if (!QStandardPaths::findExecutable("pacman").isEmpty()) suffix = "-x86_64.pkg.tar.zst";
        else return QString();
        if (QStandardPaths::findExecutable("pkexec").isEmpty()) return QString();
    } else {
        suffix = "-linux-x86_64.tar.gz";
    }
#endif
    for (const QJsonValue& v : rel.value("assets").toArray()) {
        const QJsonObject a = v.toObject();
        const QString n = a.value("name").toString();
        if (n.endsWith(suffix, Qt::CaseInsensitive) && !n.contains("-src", Qt::CaseInsensitive) &&
            !n.startsWith("whisper", Qt::CaseInsensitive)) {  // Spracherkennung fürs Untertitel-Plugin, kein Update
            *url = a.value("browser_download_url").toString();
            return n;
        }
    }
    return QString();
}

void Updater::download(const QString& url, const QString& name, const QString& page, const QString& tag) {
    const UpdaterTexts tx = o_.texts();
    const QString dir = QDir::temp().filePath(o_.appName.toLower() + "-update");
    QDir().mkpath(dir);
    const QString path = dir + "/" + name;
    auto file = std::make_shared<QFile>(path);
    if (!file->open(QIODevice::WriteOnly)) return fail(page);
    installing_ = true;

    auto* dlg = new QProgressDialog(tx.downloading, tx.cancel, 0, 1000, o_.parent ? o_.parent() : nullptr);
    dlg->setWindowTitle(o_.appName);
    dlg->setMinimumDuration(0);
    dlg->setAutoClose(false);
    dlg->setAutoReset(false);
    dlg->setValue(0);
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, o_.appName + "/" + o_.version);
    QNetworkReply* r = net_->get(req);
    connect(r, &QNetworkReply::readyRead, this, [r, file] { file->write(r->readAll()); });
    connect(r, &QNetworkReply::downloadProgress, dlg, [dlg](qint64 got, qint64 total) {
        if (total > 0) dlg->setValue(int(got * 1000 / total));
    });
    connect(dlg, &QProgressDialog::canceled, r, &QNetworkReply::abort);
    connect(r, &QNetworkReply::finished, this, [this, r, file, dlg, path, page, tag] {
        r->deleteLater();
        installing_ = busy_ = false;  // bei Erfolg beendet sich die App gleich ohnehin
        const bool cancelled = dlg->wasCanceled();
        dlg->close();
        dlg->deleteLater();
        file->write(r->readAll());
        file->close();
        if (cancelled) { QFile::remove(path); return; }
        if (r->error() != QNetworkReply::NoError || file->size() < 1024) { QFile::remove(path); return fail(page); }
        if (!launchInstaller(path)) return fail(page);
        QSettings().setValue("update/attempted", tag);
        QTimer::singleShot(0, qApp, [this] { o_.quit ? o_.quit() : QCoreApplication::quit(); });
    });
}

// Hilfsskript: wartet auf das Ende dieser App, installiert, startet neu
bool Updater::launchInstaller(const QString& file) {
    const qint64 pid = QCoreApplication::applicationPid();
    qunsetenv("UPDATER_AUTO");  // Test-Hooks nicht an die neu gestartete App vererben
    qunsetenv("UPDATER_FAKE_VERSION");
    const QString exe = QCoreApplication::applicationFilePath();
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString tmp = QFileInfo(file).absolutePath();
#if defined(Q_OS_WIN)
    QString args;
    for (const QString& a : o_.relaunchArgs) args += (args.isEmpty() ? "" : ",") + pq(a);
    QStringList s;
    const QString log = pq(QDir::toNativeSeparators(tmp + "/update.log"));
    s << "$ErrorActionPreference = 'SilentlyContinue'"
      << QString("Wait-Process -Id %1 -Timeout 20").arg(pid)
      // Hängt die alte App noch (z. B. ein Dialog), hart beenden - sonst sind ihre Dateien gesperrt
      << QString("if (Get-Process -Id %1) { Stop-Process -Id %1 -Force; Start-Sleep -Seconds 1 }").arg(pid);
    if (file.endsWith(".exe", Qt::CaseInsensitive)) {
        // Gleicher Modus und Ordner wie die bestehende Installation; "alle Benutzer" -> Setup fragt nach Adminrechten
        const QString scope = installScope(o_.innoAppId, appDir);
        s << QString("$p = Start-Process -Wait -PassThru -FilePath %1 -ArgumentList '/SILENT','/SUPPRESSMSGBOXES','/NORESTART',"
                     "'/CLOSEAPPLICATIONS','/FORCECLOSEAPPLICATIONS','%2',%3")
                 .arg(pq(QDir::toNativeSeparators(file)), scope, pq("/DIR=\"" + QDir::toNativeSeparators(appDir) + "\""))
          << QString("\"setup %1 exit=$($p.ExitCode)\" | Out-File -LiteralPath %2 -Encoding utf8").arg(scope, log);
    } else {
        const QString out = QDir::toNativeSeparators(tmp + "/unpacked");
        s << QString("Remove-Item -LiteralPath %1 -Recurse -Force").arg(pq(out))
          << QString("Expand-Archive -LiteralPath %1 -DestinationPath %2 -Force").arg(pq(QDir::toNativeSeparators(file)), pq(out))
          << QString("$src = Get-ChildItem -LiteralPath %1 -Directory | Select-Object -First 1").arg(pq(out))
          << QString("Copy-Item -Path (Join-Path $src.FullName '*') -Destination %1 -Recurse -Force")
                 .arg(pq(QDir::toNativeSeparators(appDir)));
    }
    s << QString("Start-Process -FilePath %1%2").arg(pq(QDir::toNativeSeparators(exe)),
                                                     args.isEmpty() ? QString() : " -ArgumentList " + args);
    const QString script = tmp + "/update.ps1";
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write("\xEF\xBB\xBF");  // BOM: Windows PowerShell liest sonst Umlaute im Pfad falsch
    f.write(s.join("\r\n").toUtf8());
    f.close();
    return QProcess::startDetached("powershell.exe", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-WindowStyle",
                                                      "Hidden", "-File", QDir::toNativeSeparators(script)});
#else
    QString args;
    for (const QString& a : o_.relaunchArgs) args += " " + sq(a);
    QStringList s;
    s << "#!/bin/sh" << QString("while kill -0 %1 2>/dev/null; do sleep 0.3; done").arg(pid);
#if defined(Q_OS_MACOS)
    QDir bundle(appDir);  // .../Name.app/Contents/MacOS
    bundle.cdUp();
    bundle.cdUp();
    const QString app = bundle.absolutePath();
    s << "APP=" + sq(app) << "DMG=" + sq(file) << "MNT=$(mktemp -d /tmp/update.XXXXXX)"
      << "if hdiutil attach -nobrowse -quiet -mountpoint \"$MNT\" \"$DMG\"; then"
      << "  SRC=$(ls -d \"$MNT\"/*.app | head -n 1)"
      << "  rm -rf \"$APP.old\""
      << "  if mv \"$APP\" \"$APP.old\" && ditto \"$SRC\" \"$APP\"; then"
      << "    rm -rf \"$APP.old\"; hdiutil detach -quiet \"$MNT\""
      << "    xattr -dr com.apple.quarantine \"$APP\" 2>/dev/null"
      << "  else"
      << "    [ -d \"$APP\" ] || mv \"$APP.old\" \"$APP\"; hdiutil detach -quiet \"$MNT\"; open \"$DMG\"; exit 0"
      << "  fi"
      << "else open \"$DMG\"; exit 0; fi"
      << "open \"$APP\"" + (args.isEmpty() ? QString() : " --args" + args);
#else
    if (file.endsWith(".deb")) s << "pkexec apt-get install -y --reinstall " + sq(file);
    else if (file.endsWith(".zst")) s << "pkexec pacman -U --noconfirm " + sq(file);
    else s << "tar xzf " + sq(file) + " --strip-components=1 -C " + sq(appDir);
    s << "nohup " + sq(exe) + args + " >/dev/null 2>&1 &";
#endif
    const QString script = tmp + "/update.sh";
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(s.join("\n").toUtf8() + "\n");
    f.close();
    return QProcess::startDetached("/bin/sh", {script});
#endif
}

void Updater::fail(const QString& page) {
    const UpdaterTexts tx = o_.texts();
    QMessageBox::warning(o_.parent ? o_.parent() : nullptr, o_.appName, tx.failed);
    QDesktopServices::openUrl(QUrl(page.isEmpty() ? QString("https://github.com/%1/releases/latest").arg(o_.repo) : page));
}
