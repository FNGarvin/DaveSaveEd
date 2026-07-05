# Advanced Editing Procedures

There is a ton of stuff that I don't really mess with (Gyao pets, restaurant staff levels, chest items, etc) that a real enthusiast might want to look at.  If you are interested, here is my attempt to put together a workflow for you along with the necessary steps to install the Python interpreter necessary to do the conversion.  In this example, we will be removing all Buckwheat Flour from the save.  The same steps could be used to add it, though.  If you do not know the item ID, you can probably work it out by comparing two incremental saves.

## Part 1: Workspace Setup
---
1. Create a workspace folder (e.g., `C:\DaveEdit`).
2. Copy the save file you want to edit into your workspace.
> **CRITICAL:** Make a backup copy of your original save file before proceeding.


## Part 2: Tools and Conversion
---
1. Open your workspace folder in File Explorer, type `cmd` in the address bar, and press **Enter**.
2. Run the following command to install Python:
   ```cmd
   winget install --id=Python.Python.3
   ```
   > **IMPORTANT:** After Python finishes installing, close and reopen the Command Prompt window using the same `cmd` trick in File Explorer to update your system path.
3. Download the conversion script by running:
   ```cmd
   curl -O https://raw.githubusercontent.com/FNGarvin/DaveSaveEd/refs/heads/main/encdec.py
   ```
4. Decode your save file from `.sav` to an editable `.json` format (replace `File_0.sav` with your actual filename):
   ```cmd
   python encdec.py File_0.sav
   ```


## Part 3: Editing the Data
---
1. Open the newly generated JSON file in a text editor:
   ```cmd
   notepad File_0.json
   ```
2. Use the **Find** feature (**Ctrl + F**) to locate the item ID to be removed (e.g., `1017019` for Buckwheat Flour).
3. Delete the entire JSON object for that item.  This includes the opening `{` and closing `}`, as well as the trailing comma if one exists, to maintain valid syntax.

### Example
To remove the item with ID `1017019`, delete the entire first block:

**Before:**
```json
   "33": {
      "id": 1017019,
      "stackCount": 1
   },
   "34": {
      "id": 500021,
      "stackCount": 20
   }
```

**After:**
```json
   "34": {
      "id": 500021,
      "stackCount": 20
   }
```

4. Save the changes to the `.json` file and close the editor.


## Part 4: Finalizing
---
1. Re-encode the file back to the encrypted `.sav` format in your Command Prompt:
   ```cmd
   python encdec.py File_0.json
   ```
2. Copy your modified `.sav` file from the workspace back into the original game save directory, overwriting the existing file.
3. Launch the game to verify the fix.
