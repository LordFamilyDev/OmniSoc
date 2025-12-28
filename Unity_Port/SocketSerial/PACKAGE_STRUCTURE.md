# Unity Package Structure Guide

This guide explains how to organize the SocketSerial files as a reusable Unity package.

## Recommended Package Structure

```
SocketSerial/                          # Root package folder
├── package.json                       # Package manifest
├── README.md                          # Documentation
├── CHANGELOG.md                       # Version history (optional)
├── LICENSE.md                         # License file (optional)
│
├── Runtime/                           # Required runtime scripts
│   ├── SocketSerial.cs               # Core socket class
│   ├── SocketSerialBehaviour.cs      # MonoBehaviour wrapper
│   └── SocketSerial.Runtime.asmdef   # Assembly definition
│
├── Samples~/                          # Example scenes/scripts (optional)
│   ├── SimpleChatDemo/
│   │   ├── SimpleChatDemo.cs
│   │   ├── ChatDemoScene.unity       # Example scene
│   │   └── README.md
│   └── Examples/
│       └── SocketSerialExamples.cs
│
├── Editor/                            # Editor scripts (optional)
│   ├── SocketSerialEditor.cs         # Custom inspector (future)
│   └── SocketSerial.Editor.asmdef
│
└── Documentation~/                    # Additional docs (optional)
    ├── GettingStarted.md
    └── API.md
```

## Setup Instructions

### Method 1: Embedded Package (In Project Assets)

**Best for:** Single project use, easy modification

1. Create folder structure in your Unity project:
```
YourProject/
└── Assets/
    └── SocketSerial/
        ├── package.json
        ├── Runtime/
        │   ├── SocketSerial.cs
        │   ├── SocketSerialBehaviour.cs
        │   └── SocketSerial.Runtime.asmdef
        └── Samples~/
            └── ...
```

2. Unity will automatically recognize it as a package in the Assets folder

**Pros:**
- Easy to modify and test
- Version controlled with your project
- Immediate availability

**Cons:**
- Not easily shared between projects
- Must copy to reuse

### Method 2: Local Package (Outside Project)

**Best for:** Reusing across multiple projects on the same machine

1. Create package folder outside your Unity project:
```
~/UnityPackages/
└── SocketSerial/
    ├── package.json
    ├── Runtime/
    └── ...
```

2. Add to your project via Package Manager:
   - Open Package Manager (Window > Package Manager)
   - Click '+' button > "Add package from disk..."
   - Navigate to `SocketSerial/package.json`

**OR** edit `Packages/manifest.json` manually:
```json
{
  "dependencies": {
    "com.halfstack.socketserial": "file:../../UnityPackages/SocketSerial",
    ...
  }
}
```

**Pros:**
- Reusable across projects
- Single location for updates
- Can be version controlled separately

**Cons:**
- Absolute/relative paths can break
- Must update path if moving projects

### Method 3: Git Package (Remote Repository)

**Best for:** Team collaboration, version control, distribution

1. Create a git repository with the package structure:
```bash
cd ~/git
mkdir SocketSerial
cd SocketSerial
git init

# Copy files into structure
mkdir -p Runtime Samples~/Examples
cp SocketSerial.cs Runtime/
cp SocketSerialBehaviour.cs Runtime/
cp SocketSerial.Runtime.asmdef Runtime/
cp package.json .
cp README_Unity_SocketSerial.md README.md

git add .
git commit -m "Initial commit"
git remote add origin https://github.com/yourusername/SocketSerial.git
git push -u origin main
```

2. Add to Unity project via Package Manager:
   - Open Package Manager
   - Click '+' > "Add package from git URL..."
   - Enter: `https://github.com/yourusername/SocketSerial.git`

**OR** edit `Packages/manifest.json`:
```json
{
  "dependencies": {
    "com.halfstack.socketserial": "https://github.com/yourusername/SocketSerial.git",
    ...
  }
}
```

For specific version/tag:
```json
"com.halfstack.socketserial": "https://github.com/yourusername/SocketSerial.git#v1.0.0"
```

**Pros:**
- Easy distribution
- Version control
- Team collaboration
- Can use specific versions/tags

**Cons:**
- Requires git repository
- Updates require git operations

### Method 4: Unity Asset Store / Package Registry (Professional)

**Best for:** Public distribution, professional releases

1. Follow Unity Asset Store submission guidelines
2. OR publish to Unity Package Registry (requires Unity organization)

## Quick Start (Simplest Approach)

For immediate use without package complexity:

1. Copy just these 2 files to your project:
```
Assets/
└── Scripts/
    ├── SocketSerial.cs
    └── SocketSerialBehaviour.cs
```

2. Use immediately - no package setup needed!

## Assembly Definitions Explained

The `.asmdef` files improve compilation times by creating separate assemblies.

**SocketSerial.Runtime.asmdef:**
- Contains runtime code (SocketSerial, SocketSerialBehaviour)
- Used in builds
- No Unity Editor dependencies

**Benefits:**
- Faster recompilation (only affected assemblies rebuild)
- Clear dependency boundaries
- Can be referenced by other assemblies

**Not required** - you can delete `.asmdef` files if you prefer everything in one assembly.

## Testing Your Package

### Local Testing

1. Create a test project
2. Add package via any method above
3. Create a test scene
4. Add SocketSerialBehaviour to GameObject
5. Configure and test

### Multiple Instances

To test client/server locally:

**Option A: Two build instances**
1. Build your project
2. Run the built executable
3. Run in Unity Editor simultaneously

**Option B: ParrelSync (Editor clone)**
1. Install ParrelSync from Package Manager
2. Create project clone
3. Run both Unity instances

## Version Management

Update `package.json` when making changes:

```json
{
  "version": "1.0.0",  // Major.Minor.Patch
  ...
}
```

Create git tags for versions:
```bash
git tag v1.0.0
git push origin v1.0.0
```

## Sample Files Location

**`Samples~/` folder** (note the tilde):
- Hidden from Unity by default
- Users can import via Package Manager "Samples" section
- Not included in builds unless explicitly imported

**Alternative: `Samples/` folder** (no tilde):
- Always visible in Unity
- Included in package
- Takes up space even if unused

## Recommended Workflow

**For personal use:**
1. Start with Method 1 (embedded in Assets) for development
2. Move to Method 2 (local package) once stable
3. Consider Method 3 (git) if using across many projects

**For team/distribution:**
1. Use Method 3 (git) from the start
2. Use semantic versioning
3. Tag releases properly
4. Write good commit messages

## Files Provided

You have these files ready to use:

- **SocketSerial.cs** - Core socket class
- **SocketSerialBehaviour.cs** - MonoBehaviour wrapper
- **SocketSerialExamples.cs** - Usage examples
- **SimpleChatDemo.cs** - Interactive demo
- **package.json** - Package manifest
- **SocketSerial.Runtime.asmdef** - Assembly definition
- **README_Unity_SocketSerial.md** - Full documentation

## Next Steps

1. Choose your preferred method above
2. Create the folder structure
3. Copy the files into place
4. Test in a Unity project
5. Start using in your robot VR project!

## Need Help?

Common issues:

**Package doesn't appear in Package Manager:**
- Check `package.json` is in root of package folder
- Verify JSON is valid (use JSONLint)
- Restart Unity

**Scripts don't compile:**
- Check Unity .NET version (should be .NET 4.x or Standard 2.0)
- Verify all files are in Runtime folder
- Check for missing dependencies

**Can't find scripts in project:**
- For embedded packages, look in Assets/SocketSerial
- For external packages, look in Packages/SocketSerial
- Check Package Manager shows package is loaded
