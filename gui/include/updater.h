// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef NLFORK_UPDATER_H
#define NLFORK_UPDATER_H

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QProcess;

/**
 * NLFork self-updater. Checks the latest GitHub release of the fork, downloads the Windows zip,
 * checks its SHA-256 when GitHub provides one, unpacks it next to the old files and hands over to
 * a small PowerShell script that waits for NLFork to close, copies the new files and starts it again.
 */
class NLForkUpdater : public QObject
{
	Q_OBJECT

	Q_PROPERTY(State state READ state NOTIFY stateChanged)
	Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
	Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY releaseChanged)
	Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY releaseChanged)
	Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY releaseChanged)
	Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
	Q_PROPERTY(qint64 receivedBytes READ receivedBytes NOTIFY progressChanged)
	Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)
	Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)
	Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)

public:
	enum class State {
		Idle,
		Checking,
		UpToDate,
		Available,
		Downloading,
		Verifying,
		Installing,
		Restarting,
		Error
	};
	Q_ENUM(State)

	explicit NLForkUpdater(QObject *parent = nullptr);

	State state() const { return state_; }
	QString currentVersion() const { return current_version; }
	QString latestVersion() const { return latest_version; }
	QString releaseNotes() const { return release_notes; }
	QString releaseUrl() const { return release_url; }
	double progress() const { return total_bytes > 0 ? double(received_bytes) / double(total_bytes) : 0.0; }
	qint64 receivedBytes() const { return received_bytes; }
	qint64 totalBytes() const { return total_bytes; }
	QString errorString() const { return error_string; }
	bool updateAvailable() const;

	// "1.0.10" > "1.0.9"; a leading "v" is ignored
	static int CompareVersions(const QString &a, const QString &b);

	Q_INVOKABLE void checkForUpdates();
	Q_INVOKABLE void installUpdate();
	Q_INVOKABLE void cancel();
	Q_INVOKABLE void openReleasePage();

signals:
	void stateChanged();
	void releaseChanged();
	void progressChanged();

private:
	void SetState(State state, const QString &error = QString());
	void CheckFinished();
	void DownloadFinished();
	void StartInstall();
	void HandOver();
	QString WorkDir() const;

	QNetworkAccessManager *network;
	QNetworkReply *reply = nullptr;
	QFile *download_file = nullptr;
	QProcess *extract_process = nullptr;
	State state_ = State::Idle;
	QString current_version;
	QString latest_version;
	QString release_notes;
	QString release_url;
	QUrl asset_url;
	QString asset_name;
	QString asset_sha256;
	QString extract_dir_found;
	qint64 received_bytes = 0;
	qint64 total_bytes = 0;
	QString error_string;
};

#endif // NLFORK_UPDATER_H
