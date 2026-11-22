/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/config-manager.h"
#include "common/translation.h"
#include "engines/advancedDetector.h"

#include "sherlock/detection.h"
#include "sherlock/rose_options.h"
#include "sherlock/rose_settings.h"

namespace Sherlock {

extern const ADExtraGuiOptionsMap optionsList[];

namespace {

struct RosePopUpItem {
	const char *label;
	int value;
};

struct RosePopUp {
	const char *configKey;
	const char *label;
	const char *tooltip;
	int defaultValue;
	RosePopUpItem items[5];
};

const RosePopUp ROSE_POPUPS[] = {
	{ ROSE_CONF_HIRES_SCALE, _s("Remastered art:"),
	  _s("Draw the game at a higher resolution with the remastered art pack"), 1,
	  { { _s("Off (original 640x480)"), 1 }, { _s("2x (1280x960)"), 2 }, { _s("4x (2560x1920)"), 4 }, { nullptr, 0 } } },
	{ ROSE_CONF_SPRITE_FILTER, _s("Sprite scaling:"),
	  _s("How remastered sprites are resampled when their size differs from the screen"), 2,
	  { { _s("Nearest"), 0 }, { _s("Smooth"), 1 }, { _s("Smooth, sharp interiors"), 2 }, { nullptr, 0 } } },
	{ ROSE_CONF_FONT, _s("Text font:"),
	  _s("Font for all game text at higher resolutions"), kRoseFontAuto,
	  { { _s("Automatic"), kRoseFontAuto }, { _s("Art pack font"), kRoseFontPack },
	    { _s("Liberation Serif"), kRoseFontBundled }, { _s("Original bitmap font"), kRoseFontBitmap }, { nullptr, 0 } } },
	{ nullptr, nullptr, nullptr, 0, { { nullptr, 0 } } }
};

const ADExtraGuiOptionsMap ROSE_CHECKBOXES[] = {
	{
		GAMEOPTION_ROSE_REMASTER,
		{
			_s("Per-palette character art"),
			_s("Use remastered character frames only where the pack has a variant for the room's palette"),
			ROSE_CONF_PALETTE_SAFE,
			false,
			0,
			0
		}
	},
	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

} // End of anonymous namespace

RoseTattooOptionsWidget::RoseTattooOptionsWidget(GuiObject *boss, const Common::String &name, const Common::String &domain) :
		OptionsContainerWidget(boss, name, "SherlockRoseTattooOptionsDialog", domain) {
	_guiOptions = ConfMan.get("guioptions", domain);

	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			_checkboxes[entry->option.configOption] = new GUI::CheckboxWidget(widgetsBoss(),
				_dialogLayout + "." + entry->option.configOption, _(entry->option.label), _(entry->option.tooltip));
	for (const ADExtraGuiOptionsMap *entry = ROSE_CHECKBOXES; entry->guioFlag; ++entry)
		_checkboxes[entry->option.configOption] = new GUI::CheckboxWidget(widgetsBoss(),
			_dialogLayout + "." + entry->option.configOption, _(entry->option.label), _(entry->option.tooltip));

	for (const RosePopUp *entry = ROSE_POPUPS; entry->configKey; ++entry) {
		GUI::StaticTextWidget *text = new GUI::StaticTextWidget(widgetsBoss(),
			_dialogLayout + "." + entry->configKey + "_desc", _(entry->label), _(entry->tooltip));
		text->setAlign(Graphics::kTextAlignRight);
		GUI::PopUpWidget *popUp = new GUI::PopUpWidget(widgetsBoss(), _dialogLayout + "." + entry->configKey);
		for (uint i = 0; entry->items[i].label; ++i)
			popUp->appendEntry(_(entry->items[i].label), entry->items[i].value);
		_popUps[entry->configKey] = popUp;
	}
}

void RoseTattooOptionsWidget::defineLayout(GUI::ThemeEval &layouts, const Common::String &layoutName,
		const Common::String &overlayedLayout) const {
	layouts.addDialog(layoutName, overlayedLayout);
	layouts.addLayout(GUI::ThemeLayout::kLayoutVertical).addPadding(0, 0, 0, 0);
	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			layouts.addWidget(entry->option.configOption, "Checkbox");
	for (const ADExtraGuiOptionsMap *entry = ROSE_CHECKBOXES; entry->guioFlag; ++entry)
		layouts.addWidget(entry->option.configOption, "Checkbox");
	for (const RosePopUp *entry = ROSE_POPUPS; entry->configKey; ++entry) {
		layouts.addLayout(GUI::ThemeLayout::kLayoutHorizontal).addPadding(0, 0, 0, 0);
		layouts.addWidget(Common::String(entry->configKey) + "_desc", "OptionsLabel");
		layouts.addWidget(entry->configKey, "PopUp").closeLayout();
	}
	layouts.closeLayout().closeDialog();
}

void RoseTattooOptionsWidget::load() {
	for (Common::HashMap<Common::String, GUI::CheckboxWidget *>::iterator it = _checkboxes.begin(); it != _checkboxes.end(); ++it) {
		bool state = false;
		for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
			if (it->_key == entry->option.configOption)
				state = entry->option.defaultState;
		if (ConfMan.hasKey(it->_key, _domain))
			state = ConfMan.getBool(it->_key, _domain);
		it->_value->setState(state);
	}
	for (const RosePopUp *entry = ROSE_POPUPS; entry->configKey; ++entry) {
		const int value = ConfMan.hasKey(entry->configKey, _domain) ?
			ConfMan.getInt(entry->configKey, _domain) : entry->defaultValue;
		_popUps[entry->configKey]->setSelectedTag(value);
	}
}

bool RoseTattooOptionsWidget::save() {
	for (Common::HashMap<Common::String, GUI::CheckboxWidget *>::iterator it = _checkboxes.begin(); it != _checkboxes.end(); ++it)
		ConfMan.setBool(it->_key, it->_value->getState(), _domain);
	for (const RosePopUp *entry = ROSE_POPUPS; entry->configKey; ++entry)
		ConfMan.setInt(entry->configKey, _popUps[entry->configKey]->getSelectedTag(), _domain);
	return true;
}

} // End of namespace Sherlock
