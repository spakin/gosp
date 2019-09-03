// Package helper helps demonstrate a Go Server Page's ability to access an
// external Go package.
package helper

import (
	"fmt"
	"io"
)

// Bottles outputs the lyrics to "99 Bottles of Beer on the Wall".
func Bottles(w io.Writer, from, to int) {
	for i := from; i >= to; i-- {
		switch i {
		case 1:
			fmt.Fprintln(w, `<p>1 bottle of beer on the wall,
1 bottle of beer.  If that bottle should happen to fall, no more bottles
of beer on the wall.</p>`)
		case 2:
			fmt.Fprintln(w, `<p>2 bottles of beer on the wall,
2 bottles of beer.  If one of those bottles should happen to fall, 1 bottle
of beer on the wall.</p>`)
		default:
			fmt.Fprintf(w, `<p>%d bottles of beer on the wall,
%d bottles of beer.  If one of those bottles should happen to fall,
%d bottles of beer on the wall.</p>` + "\n",
				i, i, i-1)
		}
	}
}
