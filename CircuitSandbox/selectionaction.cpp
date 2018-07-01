#include "selectionaction.hpp"
// MSVC doesn't support static inline fields properly in debug mode, so we have to resort to this.
// See bug https://developercommunity.visualstudio.com/content/problem/226484/vc2017-1564-inline-variable-in-header-multiple-con.html
CanvasState SelectionAction::clipboard;
