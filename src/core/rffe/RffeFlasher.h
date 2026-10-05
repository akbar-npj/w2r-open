// Flashes the bundled RFFE probe firmware to an ESP32 over serial.
//
// The probe's app partition is written at 0x10000 (see firmware/manifest.json). Flashing is
// delegated to esptool, which owns the ESP32 serial bootloader protocol and chip detection;
// this class just locates the tool and the firmware, drives it, and reports progress.
//
// If esptool is not installed the flasher reports a clear error rather than pretending to work.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

namespace w2r {

class RffeFlasher : public QObject {
	Q_OBJECT
  public:
	explicit RffeFlasher(QObject *parent = nullptr);
	~RffeFlasher() override;

	// Absolute path to the bundled firmware, or empty if it cannot be found. The search
	// honours W2R_RFFE_FIRMWARE, then the installed/dev locations. The file name and flash
	// offset come from the manifest next to the image.
	static QString bundledFirmwarePath();
	// Flash offset for the app partition, e.g. "0x10000" (from the manifest).
	static QString flashOffset();
	// Name of the discovered esptool executable, or empty if none was found.
	static QString esptoolProgram();

	bool isBusy() const { return m_busy; }

	// Starts flashing `port` with `firmwarePath` (defaults to the bundled image).
	void start(const QString &port, const QString &firmwarePath = QString());
	void cancel();

  signals:
	void outputLine(const QString &line);
	void progress(int percent);
	void finished(bool ok, const QString &message);

  private:
	void handleOutput();

	QProcess *m_proc = nullptr;
	bool m_busy = false;
};

} // namespace w2r
