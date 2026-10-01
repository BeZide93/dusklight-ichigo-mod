"""Run the production armor-sound callback without game assets."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/armor_audio.cpp").read_text()
start = source.index("HookAction before_link_sound(")
callback = source[start:source.index("\n}", start) + 2]
fixture = r'''
#include <cassert>
#include <cstdint>
using JAISoundID = std::uint32_t;
constexpr JAISoundID Z2SE_FN_ARMER_LIGHT_ADD = 0x3004B;
constexpr JAISoundID Z2SE_FN_ARMER_HEAVY_ADD = 0x3004C;
struct ModContext {};
struct Z2CreatureLink {};
struct Z2SoundHandlePool {};
enum HookAction { HOOK_CONTINUE, HOOK_SKIP_ORIGINAL };
Z2CreatureLink* player = nullptr;
Z2CreatureLink* Z2GetLink() { return player; }
namespace mods {
template<class T> T arg(void* args, int i) {
    return *static_cast<T*>(static_cast<void**>(args)[i]);
}
}
// CALLBACK
int main() {
    Z2CreatureLink link, other;
    Z2SoundHandlePool sentinel;
    player = &link;
    // Cover every Link voice/effect/footstep ID: only the two added rattles
    // may be skipped, and only on the player sound object.
    for (JAISoundID id = 0x10000; id < 0x40000; ++id) {
        for (auto* receiver : {&link, &other, static_cast<Z2CreatureLink*>(nullptr)}) {
            void* args[] = {&receiver, &id};
            Z2SoundHandlePool* result = &sentinel;
            auto action = before_link_sound(nullptr, args, &result, nullptr);
            const bool rattle = id == Z2SE_FN_ARMER_LIGHT_ADD || id == Z2SE_FN_ARMER_HEAVY_ADD;
            const bool skipped = receiver == player && rattle;
            assert(action == (skipped ? HOOK_SKIP_ORIGINAL : HOOK_CONTINUE));
            assert(result == (skipped ? nullptr : &sentinel));
        }
    }
    player = nullptr;
    JAISoundID id = Z2SE_FN_ARMER_LIGHT_ADD;
    Z2CreatureLink* receiver = nullptr;
    void* args[] = {&receiver, &id};
    Z2SoundHandlePool* result = &sentinel;
    assert(before_link_sound(nullptr, args, &result, nullptr) == HOOK_CONTINUE);
    assert(result == &sentinel);
}
'''.replace("// CALLBACK", callback).replace("#include <cassert>", "#include <cassert>\n#include <initializer_list>")
with tempfile.TemporaryDirectory() as directory:
    cpp, exe = Path(directory) / "test.cpp", Path(directory) / "test"
    cpp.write_text(fixture)
    subprocess.run(["c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("Armor audio passed: both rattles skipped; other sounds and receivers preserved")
