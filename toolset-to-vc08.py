VCXPROJ=".vcxproj"

v2012_PLATFORM_TOOLSET="<PlatformToolset>v110</PlatformToolset>"
v2008_PLATFORM_TOOLSET="<PlatformToolset>v90</PlatformToolset>"

from os import walk, path

if __name__=="__main__":
	count=0
	for root, dirs, files in walk("."):
		for file in files:
			if file.endswith(VCXPROJ):
				print "Replacing in", path.join(root, file), "...",
				with open(path.join(root, file), 'r') as proj:
					s=proj.read()
					s=s.replace(v2012_PLATFORM_TOOLSET, v2008_PLATFORM_TOOLSET)
				with open(path.join(root, file), 'w') as proj:
					proj.write(s)
				print "done"
				count+=1
	print "Replaced toolset in", count, "project files"