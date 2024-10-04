package main

import (
	"github.com/jurgen-kluft/ccode/ccode-base"
	cpkg "github.com/jurgen-kluft/chshg/package"
)

func main() {
	if ccode.Init() {
		pkg := cpkg.GetPackage()
		ccode.GenerateFiles(pkg)
		ccode.Generate(pkg)
	}
}
