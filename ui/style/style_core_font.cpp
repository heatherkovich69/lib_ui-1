// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/style/style_core_font.h"

#include "base/algorithm.h"
#include "ui/ui_log.h"

#include <QtCore/QMap>
#include <QtCore/QVector>
#include <QtGui/QFontInfo>
#include <QtGui/QFontDatabase>
#include <QtWidgets/QApplication>

void style_InitFontsResource() {
#ifndef DESKTOP_APP_USE_PACKAGED_FONTS
	Q_INIT_RESOURCE(fonts);
#endif // !DESKTOP_APP_USE_PACKAGED_FONTS
#ifdef Q_OS_WIN
	Q_INIT_RESOURCE(win);
#elif defined Q_OS_MAC // Q_OS_WIN
	Q_INIT_RESOURCE(mac);
#elif defined Q_OS_LINUX && !defined DESKTOP_APP_USE_PACKAGED // Q_OS_WIN || Q_OS_MAC
	Q_INIT_RESOURCE(linux);
#endif // Q_OS_WIN || Q_OS_MAC || (Q_OS_LINUX && !DESKTOP_APP_USE_PACKAGED)
}

namespace style {
namespace internal {
namespace {

QMap<QString, int> fontFamilyMap;
QVector<QString> fontFamilies;
QMap<uint32, FontData*> fontsMap;

uint32 fontKey(int size, uint32 flags, int family) {
	return (((uint32(family) << 12) | uint32(size)) << 6) | flags;
}

bool ValidateFont(const QString &familyName, int flags = 0) {
	QFont checkFont(familyName);
	checkFont.setPixelSize(13);
	checkFont.setBold(flags & style::internal::FontBold);
	checkFont.setItalic(flags & style::internal::FontItalic);
	checkFont.setUnderline(flags & style::internal::FontUnderline);
	if (flags & style::internal::FontSemibold) {
		checkFont.setStyleName("Semibold");
	}
	if (flags & style::internal::FontMonospace) {
		checkFont.setStyleHint(QFont::TypeWriter);
	}
	checkFont.setStyleStrategy(QFont::PreferQuality);
	auto realFamily = QFontInfo(checkFont).family();
	if (realFamily.trimmed().compare(familyName, Qt::CaseInsensitive)) {
		UI_LOG(("Font Error: could not resolve '%1' font, got '%2'.").arg(familyName).arg(realFamily));
		return false;
	}

	auto metrics = QFontMetrics(checkFont);
	if (!metrics.height()) {
		UI_LOG(("Font Error: got a zero height in '%1'.").arg(familyName));
		return false;
	}

	return true;
}

bool LoadCustomFont(const QString &filePath, const QString &familyName, int flags = 0) {
	auto regularId = QFontDatabase::addApplicationFont(filePath);
	if (regularId < 0) {
		UI_LOG(("Font Error: could not add '%1'.").arg(filePath));
		return false;
	}

	const auto found = [&] {
		for (auto &family : QFontDatabase::applicationFontFamilies(regularId)) {
			UI_LOG(("Font: from '%1' loaded '%2'").arg(filePath).arg(family));
			if (!family.trimmed().compare(familyName, Qt::CaseInsensitive)) {
				return true;
			}
		}
		return false;
	}();
	if (!found) {
		UI_LOG(("Font Error: could not locate '%1' font in '%2'.").arg(familyName).arg(filePath));
		return false;
	}

	return ValidateFont(familyName, flags);
}

#ifndef DESKTOP_APP_USE_PACKAGED_FONTS
enum {
	FontTypeRegular = 0,
	FontTypeRegularItalic,
	FontTypeBold,
	FontTypeBoldItalic,
	FontTypeSemibold,
	FontTypeSemiboldItalic,

	FontTypesCount,
};
QString FontTypeFilenames[FontTypesCount] = {
	"OpenSans-Regular.ttf",
	"OpenSans-Italic.ttf",
	"OpenSans-Bold.ttf",
	"OpenSans-BoldItalic.ttf",
	"OpenSans-Semibold.ttf",
	"OpenSans-SemiboldItalic.ttf",
};
QString FontTypeNames[FontTypesCount] = {
	"Open Sans",
	"Open Sans",
	"Open Sans",
	"Open Sans",
#ifdef Q_OS_WIN
	"Open Sans Semibold",
	"Open Sans Semibold",
#else // Q_OS_WIN
	"Open Sans",
	"Open Sans",
#endif // !Q_OS_WIN
};
int32 FontTypeFlags[FontTypesCount] = {
	0,
	FontItalic,
	FontBold,
	FontBold | FontItalic,
	FontSemibold,
	FontSemibold | FontItalic,
};
#endif // !DESKTOP_APP_USE_PACKAGED_FONTS

bool Started = false;

} // namespace

void StartFonts() {
	if (Started) {
		return;
	}
	Started = true;

	style_InitFontsResource();

#ifndef DESKTOP_APP_USE_PACKAGED_FONTS
	for (auto i = 0; i != FontTypesCount; ++i) {
		const auto filename = FontTypeFilenames[i];
		const auto name = FontTypeNames[i];
		const auto flags = FontTypeFlags[i];
		LoadCustomFont(":/gui/fonts/" + filename, name, flags);
	}
#endif // !DESKTOP_APP_USE_PACKAGED_FONTS

#ifdef Q_OS_WIN
	QFont::insertSubstitution("Open Sans", "MS Shell Dlg 2");
#endif // Q_OS_WIN

#ifdef Q_OS_MAC
	auto list = QStringList();
	list.append("STIXGeneral");
	list.append(".SF NS Text");
	list.append("Helvetica Neue");
	list.append("Lucida Grande");
	QFont::insertSubstitutions("Open Sans", list);
#endif // Q_OS_MAC

	QApplication::setFont(QFont("Open Sans"));
}

void destroyFonts() {
	for (auto fontData : fontsMap) {
		delete fontData;
	}
	fontsMap.clear();
}

int registerFontFamily(const QString &family) {
	auto result = fontFamilyMap.value(family, -1);
	if (result < 0) {
		result = fontFamilies.size();
		fontFamilyMap.insert(family, result);
		fontFamilies.push_back(family);
	}
	return result;
}

FontData::FontData(int size, uint32 flags, int family, Font *other)
: m(f)
, _size(size)
, _flags(flags)
, _family(family) {
	if (other) {
		memcpy(modified, other, sizeof(modified));
	} else {
		memset(modified, 0, sizeof(modified));
	}
	modified[_flags] = Font(this);

	if (_flags & FontMonospace) {
		const auto type = QFontDatabase::FixedFont;
		f.setFamily(QFontDatabase::systemFont(type).family());
		f.setStyleHint(QFont::TypeWriter);
	}

	if (_flags & FontSemibold) {
		f.setStyleName("Semibold");
#ifdef DESKTOP_APP_USE_PACKAGED_FONTS
		f.setWeight(QFont::DemiBold);
#endif
	}

	f.setPixelSize(size);
	f.setBold(_flags & FontBold);
	f.setItalic(_flags & FontItalic);
	f.setUnderline(_flags & FontUnderline);
	f.setStrikeOut(_flags & FontStrikeOut);
	f.setStyleStrategy(QFont::PreferQuality);

	m = QFontMetrics(f);
	height = m.height();
	ascent = m.ascent();
	descent = m.descent();
	spacew = width(QLatin1Char(' '));
	elidew = width("...");
}

Font FontData::bold(bool set) const {
	return otherFlagsFont(FontBold, set);
}

Font FontData::italic(bool set) const {
	return otherFlagsFont(FontItalic, set);
}

Font FontData::underline(bool set) const {
	return otherFlagsFont(FontUnderline, set);
}

Font FontData::strikeout(bool set) const {
	return otherFlagsFont(FontStrikeOut, set);
}

Font FontData::semibold(bool set) const {
	return otherFlagsFont(FontSemibold, set);
}

Font FontData::monospace(bool set) const {
	return otherFlagsFont(FontMonospace, set);
}

int FontData::size() const {
	return _size;
}

uint32 FontData::flags() const {
	return _flags;
}

int FontData::family() const {
	return _family;
}

Font FontData::otherFlagsFont(uint32 flag, bool set) const {
	int32 newFlags = set ? (_flags | flag) : (_flags & ~flag);
	if (!modified[newFlags].v()) {
		modified[newFlags] = Font(_size, newFlags, _family, modified);
	}
	return modified[newFlags];
}

Font::Font(int size, uint32 flags, const QString &family) {
	if (fontFamilyMap.isEmpty()) {
		for (uint32 i = 0, s = fontFamilies.size(); i != s; ++i) {
			fontFamilyMap.insert(fontFamilies.at(i), i);
		}
	}

	auto i = fontFamilyMap.constFind(family);
	if (i == fontFamilyMap.cend()) {
		fontFamilies.push_back(family);
		i = fontFamilyMap.insert(family, fontFamilies.size() - 1);
	}
	init(size, flags, i.value(), 0);
}

Font::Font(int size, uint32 flags, int family) {
	init(size, flags, family, 0);
}

Font::Font(int size, uint32 flags, int family, Font *modified) {
	init(size, flags, family, modified);
}

void Font::init(int size, uint32 flags, int family, Font *modified) {
	uint32 key = fontKey(size, flags, family);
	auto i = fontsMap.constFind(key);
	if (i == fontsMap.cend()) {
		i = fontsMap.insert(key, new FontData(size, flags, family, modified));
	}
	ptr = i.value();
}

} // namespace internal
} // namespace style
