#!/usr/bin/python
import os, re, sys, shutil

print ""
print "Create a new GLFM project"
print ""

#
# Gets a line of input. The input is stripped of spaces. 
# If the input is empty, default_value is returned
#
def get_input(prompt, default_value):
  value = raw_input(prompt + " [" + default_value + "]: ")
  if not value:
    return default_value
  else:
    value = value.strip()
    if len(value) == 0:
      return default_value
    else:
      return value

def name_safe(value):
  return re.match(r'^[a-zA-Z\d_]*$', value) is not None

def package_safe(value):
  # From http://stackoverflow.com/questions/5205339/regular-expression-matching-fully-qualified-java-classes
  return re.match(r'^([a-zA-Z_$][a-zA-Z\d_$]*\.)*[a-zA-Z_$][a-zA-Z\d_$]*$', value) is not None 
  
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
while True:
  app_name = get_input("App name (without spaces)", "GLFMApp")
  if name_safe(app_name):
    break
  else:
    print "Illegal name! The app name can only contain letters and numbers."

while True:
  package_name = get_input("App package name", "com.myteam." + app_name)
  if package_safe(package_name):
    break
  else:
    print "Illegal package name! The app name can only contain letters and numbers,"
    print "and each component must start with a letter."

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
  "example/platform/android/build",
  "example/platform/android/.gradle",
  "example/platform/android/.idea",
  "example/platform/emscripten/bin",
  "example/platform/ios/GLFMExample.xcodeproj/project.xcworkspace",
  "example/platform/ios/GLFMExample.xcodeproj/xcuserdata",
  )

def do_name_replace(s):
  s = s.replace("GLFMExample", app_name)
  return s
  
def do_replace(s):
  s = s.replace("GLFMExample", app_name)
  s = s.replace("com.brackeen.glfmexample", package_name)
  s = s.replace("com.brackeen.${PRODUCT_NAME:rfc1034identifier}", package_name)
  return s
  
def copy_android_buildfile(src_file, dst_file):
  with open(dst_file, "wt") as fout:
    with open(src_file, "rt") as fin:
      for line in fin:
        line = line.replace("../../../include", "../../glfm/include");
        line = line.replace("../../../src", "../../glfm/src");
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
        line = line.replace("path = ../../..;", "path = ../../glfm;")
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
      elif name == "build.gradle":
        copy_android_buildfile(src, dst)
      elif name == "project.pbxproj":
        copy_ios_project_file(src, dst)
      elif (name == "AndroidManifest.xml" or name.endswith(".plist")):
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
shutil.copytree("example/src", output_dir + "/src")
shutil.copytree("example/assets", output_dir + "/assets")

# Copy project files
copy_template("example/platform/android", output_dir + "/platform/android");
copy_template("example/platform/ios", output_dir + "/platform/ios");
copy_template("example/platform/emscripten", output_dir + "/platform/emscripten");

# Special case: create a Makefile.local for emscripten
with open(output_dir + "/platform/emscripten/Makefile.local", "wt") as fout:
  fout.write("EMSCRIPTEN_PATH = " + emsdk_path)

# Woop!
print ""
print "Done."
