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
	mainlib := denv.SetupDefaultCppLibProject("chshg", "github.com\\jurgen-kluft\\chshg")
	mainlib.Dependencies = append(mainlib.Dependencies, basepkg.GetMainLib())

	// 'chshg' unittest project
	maintest := denv.SetupDefaultCppTestProject("chshg"+"_test", "github.com\\jurgen-kluft\\chshg")
	maintest.Dependencies = append(maintest.Dependencies, unittestpkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, basepkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}
