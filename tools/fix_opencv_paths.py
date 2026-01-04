import os
import sys

def fix_paths(root_dir, old_path_part, new_path_part):
    print(f"Scanning {root_dir}...")
    print(f"Replacing '{old_path_part}' with '{new_path_part}'")
    
    count = 0
    errors = 0
    
    # Normalize slashes for search
    old_path_fwd = old_path_part.replace('\\', '/')
    old_path_back = old_path_part.replace('/', '\\')
    
    new_path_fwd = new_path_part.replace('\\', '/')
    new_path_back = new_path_part.replace('/', '\\')

    for dirpath, dirnames, filenames in os.walk(root_dir):
        for filename in filenames:
            file_path = os.path.join(dirpath, filename)
            
            # Skip obvious binary extensions or large files if needed
            if filename.lower().endswith(('.exe', '.dll', '.lib', '.obj', '.pdb', '.exp', '.bin')):
                continue
                
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                
                new_content = content
                if old_path_fwd in new_content:
                    new_content = new_content.replace(old_path_fwd, new_path_fwd)
                if old_path_back in new_content:
                    new_content = new_content.replace(old_path_back, new_path_back)
                
                if new_content != content:
                    print(f"Fixing: {file_path}")
                    with open(file_path, 'w', encoding='utf-8') as f:
                        f.write(new_content)
                    count += 1
            except Exception as e:
                print(f"Error processing {file_path}: {e}")
                errors += 1
                
    print(f"Done. Fixed {count} files. Errors: {errors}")

if __name__ == "__main__":
    # Hardcoded paths based on analysis
    root = "external/opencv/build"
    old = "C:/Users/therouxe/source/repos/sunone_main/sunone_aimbot_cpp/modules/opencv"
    new = "C:/Users/therouxe/source/repos/marcoman_ai_aimbot/external/opencv"
    
    fix_paths(root, old, new)
