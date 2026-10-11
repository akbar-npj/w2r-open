// RFFE list widgets: the shield list and the reference lists (Expected / Detected / test points).
#pragma once

#include "core/rffe/RffeModel.h"

#include <QList>
#include <QListWidget>
#include <QString>
#include <QStringList>

namespace w2r {
namespace rffe {

// Shield rows; hidden shields are marked "  (hidden)". The shield name is stored in UserRole.
class RffeShieldList : public QListWidget {
	Q_OBJECT
  public:
	explicit RffeShieldList(QWidget *parent = nullptr);

	void setShields(const QList<Shield> &shields, const QString &currentName);
	QString currentShield() const;
};

// A generic reference list (expected devices, detected devices, collected test points). When
// empty it shows a single disabled hint row instead.
class RffeReferenceList : public QListWidget {
	Q_OBJECT
  public:
	explicit RffeReferenceList(QWidget *parent = nullptr);

	void setEmptyHint(const QString &text) { m_emptyHint = text; }
	void setItems(const QStringList &items);

  private:
	QString m_emptyHint;
};

} // namespace rffe
} // namespace w2r
