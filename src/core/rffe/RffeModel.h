// RFFE feature data model + per-user persistence.
//
// Reverse-engineered from W2RSolutions.exe (rffe::TestPoint / rffe::DeviceMark /
// rffe::MarkSettings and the "rffe_testpoints.json" store). Shields belong to a board and
// carry a list of expected device ids plus the test points collected for them; the whole
// collection is kept per user under QStandardPaths::AppLocalDataLocation.
//
// The on-disk document is JSON. Key vocabulary follows the original binary (shield/label/
// hidden/added/devices/tp/ref/net + role keys clk/data/v1v8).
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace w2r {
namespace rffe {

// A signal role a test point can be mapped to. The original boardview menu offers
// "Set '<net>' as RFFE CLK" and "Set '<net>' as RFFE 1V8"; DATA appears in the store schema.
enum class Role { None, Clock, Data, V1V8 };

QString roleKey(Role r);                // "clk" | "data" | "v1v8" | ""
Role roleFromKey(const QString &key);   // inverse of roleKey

// A pad mapped as an RFFE test point on the boardview.
struct TestPoint {
	QString refDes;      // component designator, e.g. "C2406_E"
	QString net;         // net name, e.g. "PP_1V8_LDO6"
	Role role = Role::None;
};

// A device reported by the probe, mapped onto a net of the open boardview.
struct DeviceMark {
	QString deviceId;    // probe-reported identifier (firmware formats "%05d")
	QString net;         // net it was mapped to
	QString label;       // optional friendly name
};

// A shield: a named region of a board with its own expected devices and test points.
struct Shield {
	QString name;
	bool hidden = false;
	bool added = false;              // added by hand (vs. shipped catalogue)
	QStringList expectedDevices;     // expected device ids
	QList<DeviceMark> deviceMarks;   // detected devices mapped to nets
	QList<TestPoint> testPoints;
};

// Persisted RFFE mark/animation settings (original keys: rffe/markBlinkMs, rffe/markHalo,
// rffe/markHoldMs, rffe/esp32, rffe/beep).
struct MarkSettings {
	int markBlinkMs = 450;   // clamped to [150, 1400] when > 0
	bool markHalo = true;
	int markHoldMs = 0;      // clamped to [1000, 120000] when > 0
	bool esp32 = true;
	bool beep = true;

	static MarkSettings load();
	void save() const;
};

// Board-scoped shields + test points, persisted to <AppLocalData>/rffe_testpoints.json.
class Store {
  public:
	// Full path to the per-user rffe_testpoints.json (created on demand by save()).
	static QString path();

	bool load();                 // false when the file is absent or unparsable
	bool save() const;

	const QList<Shield> &shields() const { return m_shields; }
	QList<Shield> &shields() { return m_shields; }

	Shield *find(const QString &name);
	void addShield(const Shield &s);
	bool removeShield(const QString &name);
	void clear() { m_shields.clear(); }

	int visibleTestPointCount() const;
	// Distinct nets referenced by non-hidden shields' test points and device marks.
	QStringList markedNets() const;

	// Whole-document (de)serialisation; exposed for the self-test.
	QByteArray toJson() const;
	bool fromJson(const QByteArray &json, QString *error = nullptr);

  private:
	QList<Shield> m_shields;
};

} // namespace rffe
} // namespace w2r
