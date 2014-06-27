#!/usr/bin/python
import os, sys, shutil

print ""
print "Create a new GLFM project"
print ""

#
# Gets a line of input. The input is stripped of spaces. 
# If allow_spaces is False, the first word is returned.
# If the input is empty, default_value is returned
#
def get_input(prompt, default_value, allow_spaces=True):
  value = raw_input(prompt + " [" + default_value + "]: ")
  if not value:
    return default_value
  elif allow_spaces:
    value = value.strip()
    if len(value) == 0:
      return default_value
    else:
      return value
  else:
    split_values = value.split()
    if len(split_values) == 0 or not split_values[0]:
      return default_value
    else:
      return split_values[0]    

#
# Read EMSCRIPTEN_ROOT from ~/.emscripten
#
dot_emscripten = os.path.expanduser("~/.emscripten")
if os.path.exists(dot_emscripten):
  exec(open(dot_emscripten, 'r').read())
if 'EMSCRIPTEN_ROOT' in globals():
  emsdk_path = os.path.dirname(os.path.dirname(EMSCRIPTEN_ROOT))
else:
  print "Warning: Emscripten does not appear to be installed"
  emsdk_path = "~/emsdk_portable"
  
#
# Get project variables 
#
app_name = get_input("App name (without spaces)", "GLFMApp", False)
package_name = get_input("App package name", "com.glfmteam." + app_name, False)
emsdk_path = get_input("Emscripten emsdk path", emsdk_path)

#
# Find a default output dir that doesn't already exist (so that nothing is overridden)
#
output_dir = "../" + app_name
output_dir_n = 1
while os.path.exists(output_dir):
  output_dir_n += 1
  output_dir = "../" + app_name + `output_dir_n`
output_dir = get_input("Project path", output_dir)
print ""

if os.path.exists(output_dir):
  print "Project path '" + output_dir + "' already exists. Exiting."
  exit(1)

#
# Confirm creation
#
print "Project summary:"
print "  App name:", app_name
print "  App package name:", package_name
print "  Emscripten emsdk path:", emsdk_path
print "  Project path:", output_dir
confirm = get_input("Create (y/n)?", "Y")
if confirm != "Y":
  print ""
  print "Project creation canceled"
  exit(1)
  
####################################################################################################

ignored_files = (".DS_Store", "Thumbs.db", "Desktop.ini")
ignored_paths = (
  "example/android/assets",
  "example/android/bin",
  "example/android/gen",
  "example/android/libs",
  "example/android/obj",
  "example/emscripten/bin",
  "example/ios/GLFMExample.xcodeproj/project.xcworkspace",
  "example/ios/GLFMExample.xcodeproj/xcuserdata",
  )

def do_name_replace(s):
  s = s.replace("GLFMExample", app_name)
  return s
  
def do_replace(s):
  s = s.replace("GLFMExample", app_name)
  s = s.replace("com.brackeen.glfmexample", package_name)
  s = s.replace("com.brackeen.${PRODUCT_NAME:rfc1034identifier}", package_name)
  return s
  
def copy_android_makefile(src_file, dst_file):
  with open(dst_file, "wt") as fout:
    with open(src_file, "rt") as fin:
      for line in fin:
        if line.startswith("GLFM_ROOT :="):
          fout.write("GLFM_ROOT := ../../../glfm\n")
        elif line.startswith("APP_ROOT :="):
          fout.write("APP_ROOT := ../../..\n")
        else:
          fout.write(do_replace(line))
          
def copy_emscripten_makefile(src_file, dst_file):
  with open(dst_file, "wt") as fout:
    with open(src_file, "rt") as fin:
      for line in fin:
        if line.startswith("GLFM_ROOT :="):
          fout.write("GLFM_ROOT := ../../glfm\n")
        elif line.startswith("APP_ROOT :="):
          fout.write("APP_ROOT := ../..\n")
        else:
          fout.write(do_replace(line))

def copy_ios_project_file(src_file, dst_file):
  with open(dst_file, "wt") as fout:
    with open(src_file, "rt") as fin:
      for line in fin:
        line = line.replace("path = ../..;", "path = ../../glfm;")
        line = line.replace("path = ../assets;", "path = ../../assets;")
        line = line.replace("path = ../main.c;", "path = ../../main.c;")
        fout.write(do_replace(line))

def copy_generic_project_file(src_file, dst_file):
  with open(dst_file, "wt") as fout:
    with open(src_file, "rt") as fin:
      for line in fin:
        fout.write(do_replace(line))

def copy_template(src_dir, dst_dir):
  if src_dir in ignored_paths:
      return
  if not os.path.exists(dst_dir):
    os.makedirs(dst_dir)
  for name in os.listdir(src_dir):
    if name in ignored_files:
      continue
    src = os.path.join(src_dir, name)
    dst = os.path.join(dst_dir, do_name_replace(name))
    if os.path.isfile(src):
      if name == "Makefile":
        copy_emscripten_makefile(src, dst)
      elif name == "Android.mk":
        copy_android_makefile(src, dst)
      elif name == "project.pbxproj":
        copy_ios_project_file(src, dst)
      elif (name == ".project" or name == ".cproject" or 
        name.endswith(".xml") or name.endswith(".plist")):
        copy_generic_project_file(src, dst)
      else:
        shutil.copy2(src, dst)
    elif os.path.isdir(src):
      copy_template(src, dst)

os.makedirs(output_dir)

# Copy GLFM
shutil.copytree("include", output_dir + "/glfm/include")
shutil.copytree("src", output_dir + "/glfm/src")

# Copy example
shutil.copy2("example/main.c", output_dir + "/main.c")
shutil.copytree("example/assets", output_dir + "/assets")

# Copy project files
# We're renaming things and moving files around, so this is weird
copy_template("example/android", output_dir + "/platform/android");
copy_template("example/ios", output_dir + "/platform/ios");
copy_template("example/emscripten", output_dir + "/platform/emscripten");
shutil.copy2(".gitignore", output_dir + "/.gitignore")

# Special case: create a Makefile.local for emscripten
with open(output_dir + "/platform/emscripten/Makefile.local", "wt") as fout:
  fout.write("EMSCRIPTEN_PATH = " + emsdk_path)

# Woop!
print ""
print "Done."
