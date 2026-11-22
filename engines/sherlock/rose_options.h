/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_ROSE_OPTIONS_H
#define SHERLOCK_ROSE_OPTIONS_H

#include "common/hashmap.h"
#include "common/hash-str.h"
#include "gui/ThemeEval.h"
#include "gui/widget.h"
#include "gui/widgets/popup.h"

namespace Sherlock {

// Engine options tab for Rose Tattoo: the usual checkboxes plus the remaster
// settings (see rose_settings.h). The pack directory and font file are
// scummvm.ini keys (rt_asset_pack, rt_font_file); a pack in <game>/remaster is
// found automatically.
class RoseTattooOptionsWidget : public GUI::OptionsContainerWidget {
public:
	RoseTattooOptionsWidget(GuiObject *boss, const Common::String &name, const Common::String &domain);

	void load() override;
	bool save() override;

private:
	void defineLayout(GUI::ThemeEval &layouts, const Common::String &layoutName, const Common::String &overlayedLayout) const override;

	Common::String _guiOptions;
	Common::HashMap<Common::String, GUI::CheckboxWidget *> _checkboxes;
	Common::HashMap<Common::String, GUI::PopUpWidget *> _popUps;
};

} // End of namespace Sherlock

#endif
