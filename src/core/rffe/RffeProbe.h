// ESP32 RFFE probe: talks to the Way2Repair USB-serial probe over Qt SerialPort.
//
// The probe firmware (rffe-esp32-1.0.1.bin) is a small Arduino sketch. The host initiates a
// bus scan by writing the literal command "way2_rffe_"; the probe streams results back. The
// firmware carries no text response vocabulary, so we treat the reply as an opaque stream:
// every line is forwarded to the terminal, and 5-digit tokens are surfaced as detected
// device identifiers (the firmware formats identifiers with "%05d").
//
// The class compiles without QtSerialPort: serial support is reported as unavailable and all
// operations fail cleanly, so the rest of the app builds regardless.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

#ifdef W2R_HAVE_SERIALPORT
class QSerialPort;
#endif

namespace w2r {

// A candidate serial port plus the USB identity shown in the UI.
struct RffePort {
	QString name;         // system port name, e.g. "ttyUSB0" / "COM5"
	QString description;  // human-readable description
	QString manufacturer;
	QString serial;
	quint16 vid = 0;
	quint16 pid = 0;
	bool isUsb = false;

	QString label() const;
};

class RffeProbe : public QObject {
	Q_OBJECT
  public:
	// The literal scan command sent to the probe.
	static const char *scanCommand();

	explicit RffeProbe(QObject *parent = nullptr);
	~RffeProbe() override;

	static bool serialSupported();
	static QVector<RffePort> availablePorts();

	bool open(const QString &portName, int baud = 115200, QString *error = nullptr);
	void close();
	bool isOpen() const { return m_open; }
	QString portName() const { return m_portName; }
	int baudRate() const { return m_baud; }

	// Writes the scan command. Returns false (with *error set) when the port is not open.
	bool sendScan(QString *error = nullptr);
	void write(const QByteArray &data);

  signals:
	void connected(const QString &portName);
	void disconnected();
	void dataReceived(const QByteArray &data);
	void lineReceived(const QString &line);
	void deviceDetected(const QString &id);
	void errorOccurred(const QString &message);

  private slots:
	void onReadyRead();

  private:
	void consume(const QByteArray &data);

#ifdef W2R_HAVE_SERIALPORT
	QSerialPort *m_serial = nullptr;
#endif
	QByteArray m_buffer;
	bool m_open = false;
	QString m_portName;
	int m_baud = 115200;
};

} // namespace w2r
