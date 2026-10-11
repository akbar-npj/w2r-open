#include "core/rffe/RffeModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSettings>
#include <QStandardPaths>

namespace w2r {
namespace rffe {

namespace {

// The original store uses the app's per-user local data directory (StandardLocation 9).
QString storePath()
{
	QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	if (base.isEmpty())
		base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.local/share/w2r-open");
	return base + QStringLiteral("/rffe_testpoints.json");
}

QSettings settingsStore()
{
	return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("w2r-open"),
	                 QStringLiteral("w2r-open"));
}

// Headless harnesses set W2R_NO_SETTINGS so they never clobber real preferences.
bool settingsWritable() { return qEnvironmentVariableIsEmpty("W2R_NO_SETTINGS"); }

int clampIfPositive(int value, int lo, int hi)
{
	if (value <= 0) return value;
	return value < lo ? lo : (value > hi ? hi : value);
}

QJsonObject testPointToJson(const TestPoint &tp)
{
	QJsonObject o;
	if (!tp.refDes.isEmpty()) o.insert(QStringLiteral("ref"), tp.refDes);
	if (!tp.net.isEmpty()) o.insert(QStringLiteral("net"), tp.net);
	if (tp.role != Role::None) o.insert(QStringLiteral("role"), roleKey(tp.role));
	return o;
}

TestPoint testPointFromJson(const QJsonObject &o)
{
	TestPoint tp;
	tp.refDes = o.value(QStringLiteral("ref")).toString();
	tp.net = o.value(QStringLiteral("net")).toString();
	tp.role = roleFromKey(o.value(QStringLiteral("role")).toString());
	return tp;
}

QJsonObject deviceMarkToJson(const DeviceMark &d)
{
	QJsonObject o;
	o.insert(QStringLiteral("id"), d.deviceId);
	if (!d.net.isEmpty()) o.insert(QStringLiteral("net"), d.net);
	if (!d.label.isEmpty()) o.insert(QStringLiteral("label"), d.label);
	return o;
}

DeviceMark deviceMarkFromJson(const QJsonObject &o)
{
	DeviceMark d;
	d.deviceId = o.value(QStringLiteral("id")).toString();
	d.net = o.value(QStringLiteral("net")).toString();
	d.label = o.value(QStringLiteral("label")).toString();
	return d;
}

QJsonObject shieldToJson(const Shield &s)
{
	QJsonObject inner; // the "shield" sub-object holds label/hidden/added
	inner.insert(QStringLiteral("label"), s.name);
	inner.insert(QStringLiteral("hidden"), s.hidden);
	inner.insert(QStringLiteral("added"), s.added);

	QJsonArray devices;
	for (const QString &d : s.expectedDevices) devices.append(d);

	QJsonArray dev;
	for (const DeviceMark &d : s.deviceMarks) dev.append(deviceMarkToJson(d));

	QJsonArray tps;
	for (const TestPoint &tp : s.testPoints) tps.append(testPointToJson(tp));

	QJsonObject o;
	o.insert(QStringLiteral("shield"), inner);
	o.insert(QStringLiteral("devices"), devices);
	o.insert(QStringLiteral("dev"), dev);
	o.insert(QStringLiteral("tp"), tps);
	return o;
}

Shield shieldFromJson(const QJsonObject &o, const QString &fallbackName)
{
	Shield s;
	const QJsonObject inner = o.value(QStringLiteral("shield")).toObject();
	s.name = inner.value(QStringLiteral("label")).toString();
	if (s.name.isEmpty()) s.name = fallbackName;
	s.hidden = inner.value(QStringLiteral("hidden")).toBool(false);
	s.added = inner.value(QStringLiteral("added")).toBool(false);
	for (const QJsonValue &v : o.value(QStringLiteral("devices")).toArray())
		if (v.isString()) s.expectedDevices << v.toString();
	for (const QJsonValue &v : o.value(QStringLiteral("dev")).toArray())
		if (v.isObject()) s.deviceMarks << deviceMarkFromJson(v.toObject());
	for (const QJsonValue &v : o.value(QStringLiteral("tp")).toArray())
		if (v.isObject()) s.testPoints << testPointFromJson(v.toObject());
	return s;
}

} // namespace

QString roleKey(Role r)
{
	switch (r) {
	case Role::Clock: return QStringLiteral("clk");
	case Role::Data: return QStringLiteral("data");
	case Role::V1V8: return QStringLiteral("v1v8");
	case Role::None: break;
	}
	return QString();
}

Role roleFromKey(const QString &key)
{
	if (key == QStringLiteral("clk")) return Role::Clock;
	if (key == QStringLiteral("data")) return Role::Data;
	if (key == QStringLiteral("v1v8")) return Role::V1V8;
	return Role::None;
}

MarkSettings MarkSettings::load()
{
	MarkSettings s;
	const QSettings q = settingsStore();
	s.markBlinkMs = clampIfPositive(q.value(QStringLiteral("rffe/markBlinkMs"), 450).toInt(), 150, 1400);
	s.markHalo = q.value(QStringLiteral("rffe/markHalo"), true).toBool();
	s.markHoldMs = clampIfPositive(q.value(QStringLiteral("rffe/markHoldMs"), 0).toInt(), 1000, 120000);
	s.esp32 = q.value(QStringLiteral("rffe/esp32"), true).toBool();
	s.beep = q.value(QStringLiteral("rffe/beep"), true).toBool();
	return s;
}

void MarkSettings::save() const
{
	if (!settingsWritable()) return;
	QSettings q = settingsStore();
	q.setValue(QStringLiteral("rffe/markBlinkMs"), clampIfPositive(markBlinkMs, 150, 1400));
	q.setValue(QStringLiteral("rffe/markHalo"), markHalo);
	q.setValue(QStringLiteral("rffe/markHoldMs"), clampIfPositive(markHoldMs, 1000, 120000));
	q.setValue(QStringLiteral("rffe/esp32"), esp32);
	q.setValue(QStringLiteral("rffe/beep"), beep);
}

QString Store::path() { return storePath(); }

bool Store::load()
{
	QFile f(storePath());
	if (!f.exists()) return false;
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
	const QByteArray data = f.readAll();
	f.close();
	QString error;
	if (!fromJson(data, &error)) return false;
	return true;
}

bool Store::save() const
{
	const QString p = storePath();
	QDir().mkpath(QFileInfo(p).absolutePath());
	QFile f(p);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
	const QByteArray data = toJson();
	const qint64 written = f.write(data);
	f.close();
	return written == data.size();
}

Shield *Store::find(const QString &name)
{
	for (Shield &s : m_shields)
		if (s.name == name) return &s;
	return nullptr;
}

void Store::addShield(const Shield &s)
{
	if (Shield *existing = find(s.name)) {
		*existing = s;
		return;
	}
	m_shields.append(s);
}

bool Store::removeShield(const QString &name)
{
	for (int i = 0; i < m_shields.size(); ++i) {
		if (m_shields[i].name == name) {
			m_shields.removeAt(i);
			return true;
		}
	}
	return false;
}

int Store::visibleTestPointCount() const
{
	int n = 0;
	for (const Shield &s : m_shields)
		if (!s.hidden) n += s.testPoints.size();
	return n;
}

QStringList Store::markedNets() const
{
	QStringList nets;
	auto add = [&nets](const QString &net) {
		if (!net.isEmpty() && !nets.contains(net)) nets << net;
	};
	for (const Shield &s : m_shields) {
		if (s.hidden) continue;
		for (const TestPoint &tp : s.testPoints) add(tp.net);
		for (const DeviceMark &d : s.deviceMarks) add(d.net);
	}
	return nets;
}

QByteArray Store::toJson() const
{
	QJsonObject root;
	root.insert(QStringLiteral("version"), 1);
	QJsonArray shields;
	for (const Shield &s : m_shields) shields.append(shieldToJson(s));
	root.insert(QStringLiteral("shields"), shields);
	return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool Store::fromJson(const QByteArray &json, QString *error)
{
	QJsonParseError pe{};
	const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
	if (pe.error != QJsonParseError::NoError) {
		if (error) *error = pe.errorString();
		return false;
	}
	if (!doc.isObject()) {
		if (error) *error = QStringLiteral("root is not a JSON object");
		return false;
	}

	m_shields.clear();
	const QJsonObject root = doc.object();
	const QJsonArray shields = root.value(QStringLiteral("shields")).toArray();
	for (const QJsonValue &v : shields) {
		if (!v.isObject()) continue;
		m_shields << shieldFromJson(v.toObject(), QString());
	}
	return true;
}

} // namespace rffe
} // namespace w2r
