package chshg

import (
	cbase "github.com/jurgen-kluft/cbase/package"
	"github.com/jurgen-kluft/ccode/denv"
	cunittest "github.com/jurgen-kluft/cunittest/package"
)

// GetPackage returns the package object of 'chshg'
func GetPackage() *denv.Package {
	// Dependencies
	unittestpkg := cunittest.GetPackage()
	basepkg := cbase.GetPackage()

	// The main package
	mainpkg := denv.NewPackage("chshg")
	mainpkg.AddPackage(unittestpkg)
	mainpkg.AddPackage(basepkg)

	// 'chshg' library
	mainlib := denv.SetupCppLibProject("chshg", "github.com\\jurgen-kluft\\chshg")
	mainlib.AddDependencies(basepkg.GetMainLib()...)

	// 'chshg' unittest project
	maintest := denv.SetupDefaultCppTestProject("chshg"+"_test", "github.com\\jurgen-kluft\\chshg")
	maintest.AddDependencies(unittestpkg.GetMainLib()...)
	maintest.AddDependencies(basepkg.GetMainLib()...)
	maintest.Dependencies = append(maintest.Dependencies, mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}
