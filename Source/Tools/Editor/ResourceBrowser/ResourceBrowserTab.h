//
// Copyright (c) 2017-2020 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../Project/EditorTab.h"
#include "../ResourceBrowser/ResourceDragDropPayload.h"

#include <Urho3D/Utility/FileSystemReflection.h>

#include <EASTL/optional.h>

namespace Urho3D
{

class ResourceBrowserTab : public EditorTab
{
    URHO3D_OBJECT(ResourceBrowserTab, EditorTab);

public:
    explicit ResourceBrowserTab(Context* context);
    ~ResourceBrowserTab() override;

    /// Actions with current state
    /// @{
    void ScrollToSelection();
    /// @}

protected:
    void RenderContentUI() override;

private:
    struct ResourceRoot
    {
        ea::string name_;
        bool openByDefault_{};
        bool supportCompositeFiles_{};
        StringVector watchedDirectories_;
        ea::string activeDirectory_;

        SharedPtr<FileSystemReflection> reflection_;
    };

    /// Render left panel
    /// @{
    void RenderDirectoryTree(const FileSystemEntry& entry, const ea::string& displayedName);
    void RenderDirectoryContextMenu(const FileSystemEntry& entry);
    /// @}

    /// Render right panel
    /// @{
    void RenderDirectoryContent();
    void RenderDirectoryUp(const FileSystemEntry& entry);
    void RenderDirectoryContentEntry(const FileSystemEntry& entry);
    void RenderCompositeFile(const FileSystemEntry& entry);
    void RenderCompositeFileEntry(const FileSystemEntry& entry, const FileSystemEntry& ownerEntry);
    /// @}

    /// Common rendering
    /// @{
    void RenderRenameDialog(const FileSystemEntry& entry);
    /// @}

    /// Drag&drop handling
    /// @{
    SharedPtr<ResourceDragDropPayload> CreateDragDropPayload(const FileSystemEntry& entry) const;
    void BeginEntryDrag(const FileSystemEntry& entry);
    void DropPayloadToFolder(const FileSystemEntry& entry);
    /// @}

    /// Utility functions
    /// @{
    ea::string GetEntryIcon(const FileSystemEntry& entry) const;
    unsigned GetRootIndex(const FileSystemEntry& entry) const;
    const ResourceRoot& GetRoot(const FileSystemEntry& entry) const;
    bool IsEntryFromCache(const FileSystemEntry& entry) const;
    /// @}

    void SelectLeftPanel(const ea::string& path, ea::optional<unsigned> rootIndex = ea::nullopt);
    void SelectRightPanel(const ea::string& path);
    void AdjustSelectionOnRename(const ea::string& oldResourceName, const ea::string& newResourceName);

    void RefreshContents();
    void RevealInExplorer(const ea::string& path);
    void RenameEntry(const FileSystemEntry& entry, const ea::string& newName);
    void RenameOrMoveEntry(const ea::string& oldFileName, const ea::string& newFileName,
        const ea::string& oldResourceName, const ea::string& newResourceName, bool adjustSelection);

    ea::vector<ResourceRoot> roots_;

    /// UI state
    /// @{
    struct LeftPanel
    {
        unsigned selectedRoot_{1};
        ea::string selectedPath_;

        bool scrollToSelection_{};
    } left_;

    struct RightPanel
    {
        ea::string selectedPath_;

        bool scrollToSelection_{};
    } right_;

    ea::string renameBuffer_;
    bool waitingForUpdate_{};
    /// @}

    ea::vector<const FileSystemEntry*> tempEntryList_;
};

}
