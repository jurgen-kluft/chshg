package main

import (
	ccode "github.com/jurgen-kluft/ccode"
	cpkg "github.com/jurgen-kluft/chshg/package"
)

func main() {
	if ccode.Init() {
		pkg := cpkg.GetPackage()
		ccode.GenerateFiles(pkg)
		ccode.Generate(pkg)
	}
}
