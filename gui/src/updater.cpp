// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "updater.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(chiakiGui);

#ifndef NLFORK_VERSION
#define NLFORK_VERSION "0.0.0"
#endif

static const char *const RELEASES_API = "https://api.github.com/repos/CrisBuen/NLFork/releases/latest";
static const char *const RELEASES_PAGE = "https://github.com/CrisBuen/NLFork/releases/latest";
static const char *const ASSET_SUFFIX = "-windows-x64.zip";

NLForkUpdater::NLForkUpdater(QObject *parent)
	: QObject(parent), network(new QNetworkAccessManager(this)), current_version(NLFORK_VERSION)
{
	// For testing the whole flow against the latest release: pretend to be an older version
	const QByteArray fake = qgetenv("NLFORK_UPDATE_FAKE_VERSION");
	if(!fake.isEmpty())
		current_version = QString::fromUtf8(fake);
}

int NLForkUpdater::CompareVersions(const QString &a, const QString &b)
{
	auto parts = [](QString v) {
		if(v.startsWith('v') || v.startsWith('V'))
			v = v.mid(1);
		QList<int> out;
		for(const QString &p : v.split('.'))
			out.append(p.toInt());
		while(out.size() < 3)
			out.append(0);
		return out;
	};
	const auto pa = parts(a);
	const auto pb = parts(b);
	for(int i = 0; i < qMax(pa.size(), pb.size()); i++)
	{
		const int x = i < pa.size() ? pa[i] : 0;
		const int y = i < pb.size() ? pb[i] : 0;
		if(x != y)
			return x < y ? -1 : 1;
	}
	return 0;
}

bool NLForkUpdater::updateAvailable() const
{
	return !latest_version.isEmpty() && asset_url.isValid() && CompareVersions(latest_version, current_version) > 0;
}

void NLForkUpdater::SetState(State state, const QString &error)
{
	state_ = state;
	error_string = error;
	if(!error.isEmpty())
		qCWarning(chiakiGui) << "[updater]" << error;
	emit stateChanged();
}

QString NLForkUpdater::WorkDir() const
{
	return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("NLFork-update");
}

void NLForkUpdater::checkForUpdates()
{
	if(state_ == State::Checking || state_ == State::Downloading || state_ == State::Verifying ||
	   state_ == State::Installing || state_ == State::Restarting)
		return;
	QNetworkRequest request{QUrl(RELEASES_API)};
	request.setRawHeader("Accept", "application/vnd.github+json");
	request.setRawHeader("User-Agent", "NLFork-Updater");
	request.setTransferTimeout(15000);
	reply = network->get(request);
	connect(reply, &QNetworkReply::finished, this, &NLForkUpdater::CheckFinished);
	SetState(State::Checking);
}

void NLForkUpdater::CheckFinished()
{
	QNetworkReply *r = reply;
	reply = nullptr;
	r->deleteLater();
	if(r->error() != QNetworkReply::NoError)
	{
		SetState(State::Error, tr("Could not check for updates: %1").arg(r->errorString()));
		return;
	}
	const QJsonObject release = QJsonDocument::fromJson(r->readAll()).object();
	latest_version = release.value("tag_name").toString();
	if(latest_version.startsWith('v') || latest_version.startsWith('V'))
		latest_version = latest_version.mid(1);
	release_notes = release.value("body").toString();
	release_url = release.value("html_url").toString(RELEASES_PAGE);
	asset_url = QUrl();
	asset_name.clear();
	asset_sha256.clear();
	total_bytes = 0;
	for(const QJsonValue &v : release.value("assets").toArray())
	{
		const QJsonObject asset = v.toObject();
		const QString name = asset.value("name").toString();
		if(!name.endsWith(ASSET_SUFFIX, Qt::CaseInsensitive))
			continue;
		asset_name = name;
		asset_url = QUrl(asset.value("browser_download_url").toString());
		total_bytes = asset.value("size").toInteger();
		const QString digest = asset.value("digest").toString();
		if(digest.startsWith("sha256:"))
			asset_sha256 = digest.mid(7).toLower();
		break;
	}
	emit releaseChanged();
	qCInfo(chiakiGui) << "[updater] current" << current_version << "latest" << latest_version << "asset" << asset_name;
	SetState(updateAvailable() ? State::Available : State::UpToDate);
	// Test hook: install right away (used with NLFORK_UPDATE_FAKE_VERSION to test the whole flow)
	if(state_ == State::Available && qEnvironmentVariableIsSet("NLFORK_UPDATE_AUTO_INSTALL"))
		QTimer::singleShot(3000, this, &NLForkUpdater::installUpdate);
}

void NLForkUpdater::installUpdate()
{
	if(!updateAvailable() || state_ == State::Downloading || state_ == State::Verifying ||
	   state_ == State::Installing || state_ == State::Restarting)
		return;

	// The new files are copied over the current install, so that folder has to be writable
	const QString app_dir = QCoreApplication::applicationDirPath();
	QFile probe(QDir(app_dir).filePath(".nlfork-write-test"));
	if(!probe.open(QIODevice::WriteOnly))
	{
		SetState(State::Error, tr("NLFork can't write to its own folder (%1). Download the update from GitHub and extract it by hand.").arg(QDir::toNativeSeparators(app_dir)));
		return;
	}
	probe.close();
	probe.remove();

	QDir work(WorkDir());
	work.removeRecursively();
	if(!QDir().mkpath(work.path()))
	{
		SetState(State::Error, tr("Could not create the temporary folder for the update."));
		return;
	}
	if(download_file)
		download_file->deleteLater();
	download_file = new QFile(work.filePath(asset_name), this);
	if(!download_file->open(QIODevice::WriteOnly))
	{
		SetState(State::Error, tr("Could not save the update file."));
		return;
	}
	received_bytes = 0;
	emit progressChanged();

	QNetworkRequest request(asset_url);
	request.setRawHeader("User-Agent", "NLFork-Updater");
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	reply = network->get(request);
	connect(reply, &QNetworkReply::readyRead, this, [this]() {
		if(reply && download_file)
			download_file->write(reply->readAll());
	});
	connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
		received_bytes = received;
		if(total > 0)
			total_bytes = total;
		emit progressChanged();
	});
	connect(reply, &QNetworkReply::finished, this, &NLForkUpdater::DownloadFinished);
	SetState(State::Downloading);
}

void NLForkUpdater::cancel()
{
	if(state_ == State::Downloading && reply)
	{
		reply->abort();
		return; // DownloadFinished cleans up
	}
	if(state_ == State::Installing && extract_process)
	{
		extract_process->kill();
		return;
	}
	if(state_ == State::Error)
		SetState(updateAvailable() ? State::Available : State::Idle);
}

void NLForkUpdater::DownloadFinished()
{
	QNetworkReply *r = reply;
	reply = nullptr;
	r->deleteLater();
	if(download_file)
	{
		download_file->write(r->readAll());
		download_file->close();
	}
	if(r->error() != QNetworkReply::NoError)
	{
		if(download_file)
			download_file->remove();
		const bool aborted = r->error() == QNetworkReply::OperationCanceledError;
		if(aborted)
			SetState(State::Available);
		else
			SetState(State::Error, tr("The download failed: %1").arg(r->errorString()));
		return;
	}

	SetState(State::Verifying);
	if(!asset_sha256.isEmpty())
	{
		QFile f(download_file->fileName());
		QCryptographicHash hash(QCryptographicHash::Sha256);
		if(!f.open(QIODevice::ReadOnly) || !hash.addData(&f) || hash.result().toHex() != asset_sha256.toUtf8())
		{
			f.close();
			QFile::remove(download_file->fileName());
			SetState(State::Error, tr("The downloaded file is damaged (checksum mismatch). Try again."));
			return;
		}
	}
	// Let the "verifying" step show for a moment
	QTimer::singleShot(600, this, &NLForkUpdater::StartInstall);
}

void NLForkUpdater::StartInstall()
{
	// Unpack while NLFork is still open so the window can show progress; the script that runs
	// after NLFork closes only has to copy the files over.
	SetState(State::Installing);
	const QString extract_dir = QDir(WorkDir()).filePath("extract");
	QDir(extract_dir).removeRecursively();
	QDir().mkpath(extract_dir);
	extract_process = new QProcess(this);
	// tar.exe (bsdtar) ships with Windows 10 and later and unpacks zip files quickly
	extract_process->setProgram("tar.exe");
	extract_process->setArguments({"-xf", QDir::toNativeSeparators(download_file->fileName()), "-C", QDir::toNativeSeparators(extract_dir)});
	connect(extract_process, &QProcess::finished, this, [this, extract_dir](int code, QProcess::ExitStatus status) {
		extract_process->deleteLater();
		extract_process = nullptr;
		QString new_dir;
		QDirIterator it(extract_dir, {"NLFork.exe"}, QDir::Files, QDirIterator::Subdirectories);
		if(it.hasNext())
			new_dir = QFileInfo(it.next()).absolutePath();
		if(status != QProcess::NormalExit || code != 0 || new_dir.isEmpty())
		{
			SetState(State::Error, tr("Could not unpack the update."));
			return;
		}
		extract_dir_found = new_dir;
		HandOver();
	});
	connect(extract_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
		if(extract_process && extract_process->state() == QProcess::NotRunning)
			SetState(State::Error, tr("Could not unpack the update."));
	});
	extract_process->start();
}

void NLForkUpdater::HandOver()
{
	const QString work = WorkDir();
	const QString script_path = QDir(work).filePath("update.ps1");
	QFile script(script_path);
	if(!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		SetState(State::Error, tr("Could not prepare the update."));
		return;
	}
	// Waits for NLFork to close, copies the new files over the old ones and starts NLFork again.
	// Whatever happens, NLFork is started again so the user is never left without it.
	script.write(R"PS1(param([int]$ProcessId, [string]$Source, [string]$AppDir, [string]$Work)
$log = Join-Path $Work 'update.log'
"NLFork update $(Get-Date -Format s)" | Out-File $log
try { Wait-Process -Id $ProcessId -Timeout 60 -ErrorAction SilentlyContinue } catch {}
Start-Sleep -Milliseconds 700
$ok = $false
for ($i = 0; $i -lt 5 -and -not $ok; $i++) {
    robocopy $Source $AppDir /E /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-File $log -Append
    if ($LASTEXITCODE -lt 8) { $ok = $true } else { "robocopy exit $LASTEXITCODE, retrying" | Out-File $log -Append; Start-Sleep -Seconds 2 }
}
"copied: $ok" | Out-File $log -Append
Start-Process -FilePath (Join-Path $AppDir 'NLFork.exe') -WorkingDirectory $AppDir
if ($ok) { Remove-Item (Join-Path $Work 'extract') -Recurse -Force -ErrorAction SilentlyContinue; Get-ChildItem $Work -Filter *.zip | Remove-Item -Force -ErrorAction SilentlyContinue }
)PS1");
	script.close();

	SetState(State::Restarting);
	const QStringList args = {
		"-NoProfile", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden", "-File", QDir::toNativeSeparators(script_path),
		"-ProcessId", QString::number(QCoreApplication::applicationPid()),
		"-Source", QDir::toNativeSeparators(extract_dir_found),
		"-AppDir", QDir::toNativeSeparators(QCoreApplication::applicationDirPath()),
		"-Work", QDir::toNativeSeparators(work),
	};
	// Give the "restarting" animation a moment before closing
	QTimer::singleShot(1800, this, [this, args]() {
		if(!QProcess::startDetached("powershell.exe", args))
		{
			SetState(State::Error, tr("Could not start the installer."));
			return;
		}
		qCInfo(chiakiGui) << "[updater] handing over to the installer and quitting";
		QCoreApplication::quit();
	});
}

void NLForkUpdater::openReleasePage()
{
	QDesktopServices::openUrl(QUrl(release_url.isEmpty() ? QString(RELEASES_PAGE) : release_url));
}
