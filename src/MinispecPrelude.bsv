/* Minispec prelude -- automatically prepended to every Minispec file */
import Vector::*;  // In Minispec, Vector is a basic type

// Minispec doesn't separate module and interface names, so use typedefs to
// allow using some of the BSV Prelude modules with different interface names
typedef Reg#(t) RegU#(type t);
typedef Wire#(t) BypassWire#(type t);
typedef Wire#(t) DWire#(type t);
/* End of Minispec prelude */

