/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SHERLOCK_TATTOO_PLAYTEST_H
#define SHERLOCK_TATTOO_PLAYTEST_H

#include "common/array.h"
#include "common/events.h"
#include "common/file.h"
#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Sherlock {

class SherlockEngine;

namespace Tattoo {

/**
 * Scripted, headless play-test driver for Rose Tattoo.
 *
 * Enabled by SCUMMVM_SHERLOCK_TATTOO_INPUT_SCRIPT=<file>. The script is a list
 * of timed commands executed from Events::pollEvents(). Input commands are
 * pushed into the ScummVM event manager as ordinary Common::Events, so the
 * keymapper and the game's own input handling see them exactly like real
 * mouse/keyboard input. Coordinates are native screen coordinates (640x480,
 * relative to the current scroll position); they are multiplied by the hires
 * scale before being pushed, matching what the backend would deliver.
 *
 * "dump <name>" writes the presented hires composite to
 * $SCUMMVM_SHERLOCK_TATTOO_DUMP_DIR/<name>.png and appends a JSON line with
 * game state to $SCUMMVM_SHERLOCK_TATTOO_DUMP_DIR/playtest.jsonl.
 *
 * Every status line is also printed on stdout, prefixed with SHERLOCK_PLAYTEST.
 */
class Playtest {
private:
	enum Op {
		OP_WAIT, OP_WAITFRAMES, OP_MOVE, OP_DOWN, OP_UP, OP_CLICK, OP_RCLICK,
		OP_KEY, OP_WAITIDLE, OP_WAITSCENE, OP_WAITCANIM, OP_VERB, OP_OBJECTS,
		OP_DUMP, OP_DUMPFRAMES, OP_LOG, OP_QUIT
	};

	struct Command {
		Op _op;
		int _line;
		int _x, _y;		// coordinates, or generic integer arguments
		bool _right;		// for OP_DOWN / OP_UP
		Common::String _text;
		Common::KeyState _key;
		Command() : _op(OP_WAIT), _line(0), _x(0), _y(0), _right(false) {}
	};

	SherlockEngine *_vm;
	Common::Array<Command> _commands;
	uint _index;
	bool _failed;
	bool _finished;

	// State of the command currently waiting
	bool _waiting;
	uint32 _waitStartMillis;
	uint32 _waitStartFrame;
	int _stableFrames;
	uint32 _lastStableFrame;

	Common::String _dumpDir;
	Common::DumpFile *_log;
	Common::Point _mouse;		// last scripted mouse position (native screen coords)

	// State of the "dumpframes" command in flight
	bool _capturing;
	uint32 _captureLastFrame;	// last frame counter seen while capturing
	Common::Array<Graphics::ManagedSurface *> _captureFrames;
	Common::Array<uint32> _captureFrameCounters;
	Common::Array<uint32> _captureMillis;

	bool parse(const Common::String &path);
	bool parseKey(const Common::String &name, Common::KeyState &key) const;
	void fail(const Command &cmd, const Common::String &message);

	/** Execute the current command; returns true when it is complete. */
	bool step(Command &cmd);

	/** Insert expanded commands after the current one. */
	void expand(const Common::Array<Command> &cmds);

	void pushMouse(Common::EventType type, int x, int y);
	bool isIdle() const;
	int activeCAnimNumber() const;
	Common::String stateJson() const;
	void emit(const Common::String &kind, const Common::String &json);
	bool dump(const Command &cmd);
	bool dumpFrames(const Command &cmd);
	void listObjects();
public:
	Playtest(SherlockEngine *vm);
	~Playtest();

	/**
	 * Create a play-test driver if SCUMMVM_SHERLOCK_TATTOO_INPUT_SCRIPT is set.
	 * Returns nullptr otherwise, or if the script cannot be parsed (in which
	 * case an error is reported and the game is asked to quit).
	 */
	static Playtest *create(SherlockEngine *vm);

	/**
	 * Advance the script. Called at the start of every Events::pollEvents().
	 */
	void update();
};

} // End of namespace Tattoo

} // End of namespace Sherlock

#endif
