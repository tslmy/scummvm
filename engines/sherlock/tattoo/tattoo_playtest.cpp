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

#define FORBIDDEN_SYMBOL_EXCEPTION_getenv
#define FORBIDDEN_SYMBOL_EXCEPTION_printf
#define FORBIDDEN_SYMBOL_EXCEPTION_stdout
#define FORBIDDEN_SYMBOL_EXCEPTION_fflush

#include "sherlock/tattoo/tattoo_playtest.h"
#include "sherlock/tattoo/tattoo_scene.h"
#include "sherlock/tattoo/tattoo_user_interface.h"
#include "sherlock/events.h"
#include "sherlock/people.h"
#include "sherlock/screen.h"
#include "sherlock/sherlock.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "image/png.h"

namespace Sherlock {

namespace Tattoo {

// Game frames between the scripted transitions of a click or key press, so
// that each transition is seen by a separate Events::setButtonState() call.
static const int PLAYTEST_PRESS_FRAMES = 2;
// Consecutive idle game frames required by "waitidle".
static const int PLAYTEST_IDLE_FRAMES = 5;
static const int PLAYTEST_DEFAULT_TIMEOUT = 60000;

static Common::String jsonString(const Common::String &s) {
	Common::String out = "\"";
	for (uint i = 0; i < s.size(); ++i) {
		const char c = s[i];
		if (c == '"' || c == '\\')
			out += Common::String::format("\\%c", c);
		else if ((byte)c < 0x20 || (byte)c >= 0x7f)
			out += Common::String::format("\\u%04x", (byte)c);
		else
			out += c;
	}
	return out + "\"";
}

static bool parseInt(const Common::String &s, int &value) {
	if (s.empty())
		return false;
	char *end = nullptr;
	const long v = strtol(s.c_str(), &end, 10);
	if (!end || *end)
		return false;
	value = (int)v;
	return true;
}

Playtest::Playtest(SherlockEngine *vm) : _vm(vm), _index(0), _failed(false), _finished(false),
		_waiting(false), _waitStartMillis(0), _waitStartFrame(0), _stableFrames(0),
		_lastStableFrame(0), _log(nullptr), _mouse(-1, -1) {
}

Playtest::~Playtest() {
	if (_log) {
		_log->finalize();
		delete _log;
	}
}

Playtest *Playtest::create(SherlockEngine *vm) {
	const char *script = getenv("SCUMMVM_SHERLOCK_TATTOO_INPUT_SCRIPT");
	if (!script || !*script)
		return nullptr;

	Playtest *playtest = new Playtest(vm);
	const char *dumpDir = getenv("SCUMMVM_SHERLOCK_TATTOO_DUMP_DIR");
	if (dumpDir && *dumpDir) {
		playtest->_dumpDir = dumpDir;
		playtest->_log = new Common::DumpFile();
		const Common::FSNode logNode(Common::Path(playtest->_dumpDir + "/playtest.jsonl", '/'));
		if (!playtest->_log->open(logNode)) {
			printf("SHERLOCK_PLAYTEST_ERROR cannot write %s/playtest.jsonl\n", dumpDir);
			fflush(stdout);
			delete playtest->_log;
			playtest->_log = nullptr;
			playtest->_failed = true;
		}
	}

	if (!playtest->_failed && !playtest->parse(script))
		playtest->_failed = true;

	if (playtest->_failed) {
		vm->quitGame();
		playtest->_finished = true;
	} else {
		playtest->emit("START", Common::String::format("{\"script\":%s,\"commands\":%u}",
			jsonString(script).c_str(), playtest->_commands.size()));
	}
	return playtest;
}

bool Playtest::parseKey(const Common::String &nameIn, Common::KeyState &key) const {
	Common::String name = nameIn;
	name.toLowercase();

	static const struct {
		const char *name;
		Common::KeyCode code;
		uint16 ascii;
	} NAMED_KEYS[] = {
		{ "escape", Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE },
		{ "esc", Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE },
		{ "return", Common::KEYCODE_RETURN, Common::ASCII_RETURN },
		{ "enter", Common::KEYCODE_RETURN, Common::ASCII_RETURN },
		{ "space", Common::KEYCODE_SPACE, Common::ASCII_SPACE },
		{ "tab", Common::KEYCODE_TAB, Common::ASCII_TAB },
		{ "backspace", Common::KEYCODE_BACKSPACE, Common::ASCII_BACKSPACE },
		{ "up", Common::KEYCODE_UP, 0 },
		{ "down", Common::KEYCODE_DOWN, 0 },
		{ "left", Common::KEYCODE_LEFT, 0 },
		{ "right", Common::KEYCODE_RIGHT, 0 },
		{ "home", Common::KEYCODE_HOME, 0 },
		{ "end", Common::KEYCODE_END, 0 },
		{ "pageup", Common::KEYCODE_PAGEUP, 0 },
		{ "pagedown", Common::KEYCODE_PAGEDOWN, 0 },
		{ nullptr, Common::KEYCODE_INVALID, 0 }
	};
	for (int i = 0; NAMED_KEYS[i].name; ++i) {
		if (name == NAMED_KEYS[i].name) {
			key = Common::KeyState(NAMED_KEYS[i].code, NAMED_KEYS[i].ascii);
			return true;
		}
	}

	if (name.size() >= 2 && name[0] == 'f') {
		int n;
		if (parseInt(Common::String(name.c_str() + 1), n) && n >= 1 && n <= 12) {
			key = Common::KeyState((Common::KeyCode)(Common::KEYCODE_F1 + n - 1),
				(uint16)(Common::ASCII_F1 + n - 1));
			return true;
		}
	}

	if (name.size() == 1 && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= '0' && name[0] <= '9'))) {
		key = Common::KeyState((Common::KeyCode)name[0], (uint16)name[0]);
		return true;
	}
	return false;
}

bool Playtest::parse(const Common::String &path) {
	Common::File file;
	if (!file.open(Common::FSNode(Common::Path(path, '/')))) {
		printf("SHERLOCK_PLAYTEST_ERROR cannot read input script %s\n", path.c_str());
		fflush(stdout);
		return false;
	}

	int lineNum = 0;
	while (!file.eos() && !file.err()) {
		Common::String line = file.readLine();
		++lineNum;
		const uint hash = line.findFirstOf('#');
		if (hash != Common::String::npos)
			line = Common::String(line.c_str(), hash);
		line.trim();
		if (line.empty())
			continue;

		Common::StringArray tok;
		for (const char *p = line.c_str(); *p;) {
			while (*p == ' ' || *p == '\t')
				++p;
			const char *start = p;
			while (*p && *p != ' ' && *p != '\t')
				++p;
			if (p > start)
				tok.push_back(Common::String(start, p));
		}

		Command cmd;
		cmd._line = lineNum;
		Common::String op = tok[0];
		op.toLowercase();
		const uint args = tok.size() - 1;
		bool ok = true;

		// Rest of the line after the opcode, for free-text arguments
		Common::String rest;
		for (uint i = 1; i < tok.size(); ++i)
			rest += (i > 1 ? " " : "") + tok[i];

		if (op == "wait" || op == "waitframes") {
			cmd._op = op == "wait" ? OP_WAIT : OP_WAITFRAMES;
			ok = args == 1 && parseInt(tok[1], cmd._x) && cmd._x >= 0;
		} else if (op == "move" || op == "click" || op == "rclick") {
			cmd._op = op == "move" ? OP_MOVE : op == "click" ? OP_CLICK : OP_RCLICK;
			ok = args == 2 && parseInt(tok[1], cmd._x) && parseInt(tok[2], cmd._y) &&
				cmd._x >= 0 && cmd._x < SHERLOCK_SCREEN_WIDTH && cmd._y >= 0 && cmd._y < SHERLOCK_SCREEN_HEIGHT;
		} else if (op == "key") {
			cmd._op = OP_KEY;
			cmd._x = 2;
			ok = args == 1 && parseKey(tok[1], cmd._key);
		} else if (op == "waitidle" || op == "waitcanim") {
			cmd._op = op == "waitidle" ? OP_WAITIDLE : OP_WAITCANIM;
			cmd._x = PLAYTEST_DEFAULT_TIMEOUT;
			ok = args == 0 || (args == 1 && parseInt(tok[1], cmd._x) && cmd._x > 0);
		} else if (op == "waitscene") {
			cmd._op = OP_WAITSCENE;
			cmd._y = PLAYTEST_DEFAULT_TIMEOUT;
			ok = (args == 1 || args == 2) && parseInt(tok[1], cmd._x) &&
				(args == 1 || (parseInt(tok[2], cmd._y) && cmd._y > 0));
		} else if (op == "verb") {
			cmd._op = OP_VERB;
			cmd._text = rest;
			ok = !rest.empty();
		} else if (op == "objects") {
			cmd._op = OP_OBJECTS;
			ok = args == 0;
		} else if (op == "dump") {
			cmd._op = OP_DUMP;
			cmd._text = args == 1 ? tok[1] : "";
			ok = args == 1 && !_dumpDir.empty();
			for (uint i = 0; ok && i < cmd._text.size(); ++i) {
				const char c = cmd._text[i];
				ok = Common::isAlnum(c) || c == '-' || c == '_' || (c == '.' && i > 0);
			}
		} else if (op == "log") {
			cmd._op = OP_LOG;
			cmd._text = rest;
		} else if (op == "quit") {
			cmd._op = OP_QUIT;
			ok = args == 0;
		} else {
			ok = false;
		}

		if (!ok) {
			printf("SHERLOCK_PLAYTEST_ERROR %s:%d: invalid command: %s%s\n", path.c_str(), lineNum, line.c_str(),
				op == "dump" && _dumpDir.empty() ? " (SCUMMVM_SHERLOCK_TATTOO_DUMP_DIR is not set)" : "");
			fflush(stdout);
			return false;
		}
		_commands.push_back(cmd);
	}
	return true;
}

void Playtest::emit(const Common::String &kind, const Common::String &json) {
	printf("SHERLOCK_PLAYTEST_%s %s\n", kind.c_str(), json.c_str());
	fflush(stdout);
	if (_log) {
		_log->writeString(Common::String::format("{\"event\":%s,\"data\":%s}\n",
			jsonString(kind).c_str(), json.c_str()));
		_log->flush();
	}
}

void Playtest::fail(const Command &cmd, const Common::String &message) {
	const bool firstFailure = !_failed;
	_failed = _finished = true;
	emit("ERROR", Common::String::format("{\"line\":%d,\"message\":%s,\"state\":%s}",
		cmd._line, jsonString(message).c_str(), stateJson().c_str()));
	// Show what the screen looked like when the script gave up.
	if (firstFailure && !_dumpDir.empty()) {
		Command errorDump = cmd;
		errorDump._op = OP_DUMP;
		errorDump._text = Common::String::format("error_line%d", cmd._line);
		dump(errorDump);
	}
	_vm->quitGame();
}

void Playtest::expand(const Common::Array<Command> &cmds) {
	for (uint i = 0; i < cmds.size(); ++i)
		_commands.insert_at(_index + 1 + i, cmds[i]);
}

void Playtest::pushMouse(Common::EventType type, int x, int y) {
	const int scale = MAX(_vm->_screen->roseTattooHiresScale(), 1);
	Common::Event event;
	event.type = type;
	event.mouse = Common::Point(x * scale, y * scale);
	g_system->getEventManager()->pushEvent(event);
	_mouse = Common::Point(x, y);
}

int Playtest::activeCAnimNumber() const {
	if (!IS_ROSE_TATTOO)
		return -1;
	return static_cast<TattooScene *>(_vm->_scene)->activeCAnimNumber();
}

bool Playtest::isIdle() const {
	Person &holmes = (*_vm->_people)[HOLMES];
	// _canLoadSave is only true while SherlockEngine::handleInput() polls from
	// the top-level scene loop in STD/LAB mode with the cursor visible: i.e.
	// not inside a talk script, canimation, menu, journal or transition.
	return _vm->_canLoadSave && _vm->_scene->_goToScene == -1 && activeCAnimNumber() == -1 &&
		holmes._walkCount == 0 && holmes._walkTo.empty();
}

Common::String Playtest::stateJson() const {
	Scene &scene = *_vm->_scene;
	Screen &screen = *_vm->_screen;
	Events &events = *_vm->_events;
	Person &holmes = (*_vm->_people)[HOLMES];

	Common::String canim = "null";
	const int cAnimNum = activeCAnimNumber();
	if (cAnimNum != -1) {
		TattooScene &tScene = static_cast<TattooScene &>(scene);
		const Common::String name = cAnimNum < (int)scene._cAnim.size() ? scene._cAnim[cAnimNum]._name : "";
		canim = Common::String::format("{\"number\":%d,\"name\":%s,\"frame\":%d}", cAnimNum,
			jsonString(name).c_str(), tScene._activeCAnim.frameNumber());
	}

	return Common::String::format("{\"millis\":%u,\"frame\":%u,\"scene\":%d,\"go_to_scene\":%d,"
		"\"holmes\":[%d,%d],\"holmes_walking\":%s,\"canim\":%s,\"menu_mode\":%d,\"idle\":%s,"
		"\"mouse\":[%d,%d],\"scroll\":[%d,%d],\"hires_scale\":%d}",
		g_system->getMillis(), events.getFrameCounter(), scene._currentScene, scene._goToScene,
		holmes._position.x / FIXED_INT_MULTIPLIER, holmes._position.y / FIXED_INT_MULTIPLIER,
		holmes._walkCount > 0 || !holmes._walkTo.empty() ? "true" : "false", canim.c_str(),
		(int)_vm->_ui->_menuMode, isIdle() ? "true" : "false",
		events.screenMousePos().x, events.screenMousePos().y,
		screen._currentScroll.x, screen._currentScroll.y, screen.roseTattooHiresScale());
}

bool Playtest::dump(const Command &cmd) {
	Screen &screen = *_vm->_screen;
	const Common::String file = _dumpDir + "/" + cmd._text + ".png";
	Common::DumpFile out;
	if (!out.open(Common::FSNode(Common::Path(file, '/')))) {
		fail(cmd, "cannot write " + file);
		return false;
	}

	bool written;
	const Graphics::ManagedSurface &composite = screen.roseTattooHiresComposite();
	const bool hires = screen.roseTattooHiresScale() > 1 && !composite.empty();
	if (hires) {
		written = Image::writePNG(out, composite.rawSurface());
	} else {
		// Native rendering: the presented frame is the 8-bit screen surface.
		byte palette[Graphics::PALETTE_SIZE];
		screen.getPalette(palette);
		written = Image::writePNG(out, screen.rawSurface(), palette);
	}
	out.finalize();
	if (!written) {
		fail(cmd, "PNG encoding failed for " + file);
		return false;
	}

	emit("DUMP", Common::String::format("{\"name\":%s,\"file\":%s,\"line\":%d,\"source\":%s,\"state\":%s}",
		jsonString(cmd._text).c_str(), jsonString(file).c_str(), cmd._line,
		hires ? "\"hires_composite\"" : "\"native_screen\"", stateJson().c_str()));
	return true;
}

void Playtest::listObjects() {
	Scene &scene = *_vm->_scene;
	const Common::Point scroll = _vm->_screen->_currentScroll;
	Common::String list = "[";
	for (uint idx = 0; idx < scene._bgShapes.size(); ++idx) {
		Object &obj = scene._bgShapes[idx];
		if (obj._type == INVALID || obj._type == HIDDEN || obj._type == REMOVE)
			continue;
		// Hotspot-only objects have no frame; their clickable area is _noShapeSize.
		Common::Rect r = obj._type == NO_SHAPE ? Common::Rect(obj._position.x, obj._position.y,
			obj._position.x + obj._noShapeSize.x, obj._position.y + obj._noShapeSize.y) : obj.getNewBounds();
		r.translate(-scroll.x, -scroll.y);
		list += Common::String::format("%s{\"index\":%u,\"name\":%s,\"type\":%d,\"screen_rect\":[%d,%d,%d,%d]}",
			list.size() > 1 ? "," : "", idx, jsonString(obj._name).c_str(), (int)obj._type,
			r.left, r.top, r.right, r.bottom);
	}
	list += "]";
	emit("OBJECTS", Common::String::format("{\"scene\":%d,\"objects\":%s}", scene._currentScene, list.c_str()));
}

bool Playtest::step(Command &cmd) {
	Events &events = *_vm->_events;
	const uint32 now = g_system->getMillis();
	const uint32 frame = events.getFrameCounter();

	if (!_waiting) {
		_waiting = true;
		_waitStartMillis = now;
		_waitStartFrame = frame;
		_stableFrames = 0;
		_lastStableFrame = frame;
	}
	const uint32 elapsed = now - _waitStartMillis;

	switch (cmd._op) {
	case OP_WAIT:
		return elapsed >= (uint32)cmd._x;

	case OP_WAITFRAMES:
		return frame - _waitStartFrame >= (uint32)cmd._x;

	case OP_MOVE:
		pushMouse(Common::EVENT_MOUSEMOVE, cmd._x, cmd._y);
		return true;

	case OP_DOWN:
	case OP_UP:
		pushMouse(cmd._op == OP_DOWN ?
			(cmd._right ? Common::EVENT_RBUTTONDOWN : Common::EVENT_LBUTTONDOWN) :
			(cmd._right ? Common::EVENT_RBUTTONUP : Common::EVENT_LBUTTONUP), cmd._x, cmd._y);
		return true;

	case OP_CLICK:
	case OP_RCLICK: {
		// Move first, let the game see the hover for a moment (verb menu
		// highlighting, tooltips), then press and release on separate frames.
		Common::Array<Command> seq;
		Command c = cmd;
		c._op = OP_WAITFRAMES;
		c._x = PLAYTEST_PRESS_FRAMES;
		Command move = cmd, down = cmd, up = cmd;
		move._op = OP_MOVE;
		down._op = OP_DOWN;
		up._op = OP_UP;
		down._right = up._right = cmd._op == OP_RCLICK;
		seq.push_back(c);
		seq.push_back(down);
		seq.push_back(c);
		seq.push_back(up);
		seq.push_back(c);
		pushMouse(Common::EVENT_MOUSEMOVE, cmd._x, cmd._y);
		expand(seq);
		return true;
	}

	case OP_KEY: {
		Common::Event event;
		event.type = cmd._x == 0 ? Common::EVENT_KEYUP : Common::EVENT_KEYDOWN;
		event.kbd = cmd._key;
		g_system->getEventManager()->pushEvent(event);
		if (cmd._x == 2) {
			Common::Array<Command> seq;
			Command c = cmd, up = cmd;
			c._op = OP_WAITFRAMES;
			c._x = PLAYTEST_PRESS_FRAMES;
			up._x = 0;
			seq.push_back(c);
			seq.push_back(up);
			seq.push_back(c);
			expand(seq);
		}
		return true;
	}

	case OP_WAITIDLE:
		if (!isIdle()) {
			_stableFrames = 0;
		} else if (frame != _lastStableFrame) {
			++_stableFrames;
		}
		_lastStableFrame = frame;
		if (_stableFrames >= PLAYTEST_IDLE_FRAMES)
			return true;
		if (elapsed >= (uint32)cmd._x)
			fail(cmd, Common::String::format("waitidle timed out after %d ms", cmd._x));
		return false;

	case OP_WAITSCENE:
		if (_vm->_scene->_currentScene == cmd._x && _vm->_scene->_goToScene == -1 && _vm->_canLoadSave)
			return true;
		if (elapsed >= (uint32)cmd._y)
			fail(cmd, Common::String::format("waitscene %d timed out after %d ms", cmd._x, cmd._y));
		return false;

	case OP_WAITCANIM:
		if (activeCAnimNumber() != -1)
			return true;
		if (elapsed >= (uint32)cmd._x)
			fail(cmd, Common::String::format("waitcanim timed out after %d ms", cmd._x));
		return false;

	case OP_VERB: {
		if (!IS_ROSE_TATTOO) {
			fail(cmd, "verb requires Rose Tattoo");
			return false;
		}
		TattooUserInterface &ui = *static_cast<TattooUserInterface *>(_vm->_ui);
		Common::Point pt;
		if (!ui._verbsWidget.verbPosition(cmd._text, pt)) {
			Common::String verbs;
			for (uint i = 0; i < ui._verbsWidget._verbCommands.size(); ++i)
				verbs += (i ? ", " : "") + ui._verbsWidget._verbCommands[i];
			fail(cmd, Common::String::format("verb \"%s\" is not in an open verb menu (menu: %s)",
				cmd._text.c_str(), ui._verbsWidget.active() ? verbs.c_str() : "closed"));
			return false;
		}
		pt -= _vm->_screen->_currentScroll;
		Command click = cmd;
		click._op = OP_CLICK;
		click._x = pt.x;
		click._y = pt.y;
		emit("VERB", Common::String::format("{\"line\":%d,\"verb\":%s,\"point\":[%d,%d]}",
			cmd._line, jsonString(cmd._text).c_str(), pt.x, pt.y));
		Common::Array<Command> seq;
		seq.push_back(click);
		expand(seq);
		return true;
	}

	case OP_OBJECTS:
		listObjects();
		return true;

	case OP_DUMP:
		dump(cmd);
		return true;

	case OP_LOG:
		emit("LOG", Common::String::format("{\"line\":%d,\"text\":%s,\"state\":%s}",
			cmd._line, jsonString(cmd._text).c_str(), stateJson().c_str()));
		return true;

	case OP_QUIT:
		emit("QUIT", Common::String::format("{\"line\":%d,\"state\":%s}", cmd._line, stateJson().c_str()));
		_finished = true;
		_vm->quitGame();
		return true;
	}
	return true;
}

void Playtest::update() {
	// Polling can start before the engine has created its subsystems.
	if (!_vm->_people || !_vm->_scene || !_vm->_ui || !_vm->_screen)
		return;

	// Run every command that can complete in this poll; stop at the first
	// one that must wait for time, frames or game state.
	while (!_finished && _index < _commands.size()) {
		if (!step(_commands[_index]))
			return;
		if (_finished)
			return;
		_waiting = false;
		++_index;
		// Keep each button transition in its own poll: Events::pollEvents()
		// handles at most one button event per call.
		const Op op = _commands[_index - 1]._op;
		if (op == OP_DOWN || op == OP_UP || op == OP_KEY)
			return;
	}
	if (!_finished && _index >= _commands.size()) {
		_finished = true;
		emit("END", Common::String::format("{\"state\":%s}", stateJson().c_str()));
	}
}

} // End of namespace Tattoo

} // End of namespace Sherlock
