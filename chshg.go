package main

import (
	"github.com/jurgen-kluft/ccode/ccode-base"
	c "github.com/jurgen-kluft/chshg/package"
)

func main() {
	ccode.Init()
	ccode.GenerateFiles()
	ccode.Generate(c.GetPackage())
}
