A mostly non-functional terminal emulator meant to investigate the feasibility
of a Linux-based version of Casey Muratori's refterm[1] as well as to
personally explore the concepts presented in his series of refterm videos[2].
The rudimentaries are mostly here, so this might be useful to see how to get
the most bare-bones terminal emulator up and running, but the vast majority of
work would seem to involve supporting character escape codes (as described in
ECMA-48 or ISO/IEC 6429) that most terminal applications would probably expect
a modern terminal emulator to support.

Which is where this exploration stops (and essentially where refterm also
stops, albeit with hardware rendering and Unicode support). While I can't
disagree with the specifics of what Mr. Muratori outlines in his videos --
indeed, I would probably agree that many of his ideas should be used as a
baseline reference were I inclined to implement a full-fledged terminal
emulator -- the issue is that, once one supports arbitrary repositioning of the
terminal cursor and (over)writing of the display (features not implemented in
his refterm or addressed in the videos, although presumably this objection was
also never presented as to him as a potential impediment to performance), the
problem fundamentally changes from implementing a text streaming device to,
well, emulating a character cell display.

Which is not to disagree that the typical terminal emulator is probably
significantly under-performant, couldn't provide better support for non-ASCII
character sets, and probably shouldn't randomly choke when "fuzzed" with a
large/binary input file (issues well-addressed in his videos), but it does seem
to me that parsing and processing the input stream -- i.e., the bulk of a
terminal emulator's work -- cannot be as easily streamlined or optimized as
described in his videos. Consequently, while readily agreeing that
incorporating refterm's concepts into a full terminal emulator implementation
would be wise, I would similarly hesitate to use refterm's benchmark numbers as
a baseline for performance.

Which is also not to definitively state (at risk of becoming another "float in
the excuse parade") that significant optimization isn't possible. The jury is
still out, as far as I'm concerned. But exploring that avenue would probably
require more-or-less implementing a full-fledged terminal emulator. But my
exploration only went on as long as I had an itch to scratch.

Which is why, like a null pointer and unlike refterm, this "terminal" should
definitely not be referenced by anyone.

[1] https://github.com/cmuratori/refterm
[2] https://www.youtube.com/playlist?list=PLEMXAbCVnmY6zCgpCFlgggRkrp0tpWfrn
