// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/task_manager_mac.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/feature_list.h"
#include "base/mac/bundle_locations.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#import "chrome/browser/ui/cocoa/window_size_autosaver.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cocoa/controls/button_utils.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

NSString* ColumnIdentifier(int id) {
  return [NSString stringWithFormat:@"%d", id];
}

}  // namespace

@interface TaskManagerWindowController (Private)
- (base::scoped_nsobject<NSWindow>)createAndLayOutWindow;
- (NSTableColumn*)addColumnWithData:
    (const task_manager::TableColumnData&)columnData;
- (void)setUpTableColumns;
- (void)setUpTableHeaderContextMenu;
- (void)toggleColumn:(id)sender;
- (void)adjustSelectionAndEndProcessButton;
- (void)deselectRows;
@end

////////////////////////////////////////////////////////////////////////////////
// TaskManagerWindowController implementation:

@implementation TaskManagerWindowController

- (id)initWithTaskManagerMac:(task_manager::TaskManagerMac*)taskManagerMac
                  tableModel:(task_manager::TaskManagerTableModel*)tableModel {
  base::scoped_nsobject<NSWindow> window = [self createAndLayOutWindow];
  if ((self = [super initWithWindow:window])) {
    _taskManagerMac = taskManagerMac;
    _tableModel = tableModel;

    [window setDelegate:self];

    [_tableView setDelegate:self];
    [_tableView setDataSource:self];

    if (g_browser_process && g_browser_process->local_state()) {
      _size_saver.reset([[WindowSizeAutosaver alloc]
          initWithWindow:[self window]
             prefService:g_browser_process->local_state()
                    path:prefs::kTaskManagerWindowPlacement]);
    }
    [[self window] setExcludedFromWindowsMenu:YES];

    [self setUpTableColumns];
    [self setUpTableHeaderContextMenu];
    [self adjustSelectionAndEndProcessButton];
    [_tableView sizeToFit];

    [self reloadData];
    [self showWindow:self];
  }
  return self;
}

- (void)sortShuffleArray {
  _viewToModelMap.resize(_tableModel->RowCount());
  for (size_t i = 0; i < _viewToModelMap.size(); ++i)
    _viewToModelMap[i] = i;

  if (_currentSortDescriptor.sorted_column_id != -1) {
    task_manager::TaskManagerTableModel* tableModel = _tableModel;
    task_manager::TableSortDescriptor currentSortDescriptor =
        _currentSortDescriptor;
    std::stable_sort(_viewToModelMap.begin(), _viewToModelMap.end(),
                     [tableModel, currentSortDescriptor](int a, int b) {
                       int aStart, aLength;
                       tableModel->GetRowsGroupRange(a, &aStart, &aLength);
                       int bStart, bLength;
                       tableModel->GetRowsGroupRange(b, &bStart, &bLength);
                       if (aStart == bStart) {
                         // The two rows are in the same group, sort so that
                         // items in the same group always appear in the same
                         // order. The sort descriptor's ascending value is
                         // intentionally ignored.
                         return a < b;
                       }

                       // Sort by the first entry of each of the groups.
                       int cmp_result = tableModel->CompareValues(
                           aStart, bStart,
                           currentSortDescriptor.sorted_column_id);
                       if (!currentSortDescriptor.is_ascending)
                         cmp_result = -cmp_result;
                       return cmp_result < 0;
                     });
  }

  _modelToViewMap.resize(_viewToModelMap.size());
  for (size_t i = 0; i < _viewToModelMap.size(); ++i)
    _modelToViewMap[_viewToModelMap[i]] = i;
}

- (void)reloadData {
  [self reloadDataWithRows:0 addedAtIndex:0];
}

- (void)reloadDataWithRows:(int)addedRows addedAtIndex:(int)addedRowIndex {
  // Store old view indices, and the model indices they map to.
  NSIndexSet* viewSelection = [_tableView selectedRowIndexes];
  std::vector<int> modelSelection;
  for (NSUInteger i = [viewSelection lastIndex];
       i != NSNotFound;
       i = [viewSelection indexLessThanIndex:i]) {
    modelSelection.push_back(_viewToModelMap[i]);
  }

  // Adjust for any added or removed rows.
  if (addedRows != 0) {
    for (int& selectedItem : modelSelection) {
      if (addedRowIndex > selectedItem) {
        // Nothing to do; added/removed items are beyond the selected item.
        continue;
      }

      if (addedRows > 0) {
        selectedItem += addedRows;
      } else {
        int removedRows = -addedRows;
        if (addedRowIndex + removedRows <= selectedItem)
          selectedItem -= removedRows;
        else
          selectedItem = -1;  // The item was removed.
      }
    }
  }

  // Sort.
  [self sortShuffleArray];

  // Reload the data.
  [_tableView reloadData];

  // Reload the selection.
  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
  for (auto selectedItem : modelSelection) {
    if (selectedItem != -1)
      [indexSet addIndex:_modelToViewMap[selectedItem]];
  }
  [_tableView selectRowIndexes:indexSet byExtendingSelection:NO];

  [self adjustSelectionAndEndProcessButton];
}

- (task_manager::TableSortDescriptor)sortDescriptor {
  return _currentSortDescriptor;
}

- (void)setSortDescriptor:
    (const task_manager::TableSortDescriptor&)sortDescriptor {
  base::scoped_nsobject<NSSortDescriptor> nsSortDescriptor(
      [[NSSortDescriptor alloc]
          initWithKey:ColumnIdentifier(sortDescriptor.sorted_column_id)
            ascending:sortDescriptor.is_ascending]);
  [_tableView setSortDescriptors:@[ nsSortDescriptor ]];
}

- (BOOL)visibilityOfColumnWithId:(int)columnId {
  NSTableColumn* column =
      [_tableView tableColumnWithIdentifier:ColumnIdentifier(columnId)];
  return column ? ![column isHidden] : NO;
}

- (void)setColumnWithId:(int)columnId toVisibility:(BOOL)visibility {
  NSTableColumn* column =
      [_tableView tableColumnWithIdentifier:ColumnIdentifier(columnId)];
  [column setHidden:!visibility];

  [_tableView sizeToFit];
  [_tableView setNeedsDisplay];
}

- (IBAction)killSelectedProcesses:(id)sender {
  NSIndexSet* selection = [_tableView selectedRowIndexes];
  for (NSUInteger i = [selection lastIndex];
       i != NSNotFound;
       i = [selection indexLessThanIndex:i]) {
    _tableModel->KillTask(_viewToModelMap[i]);
  }
}

- (void)tableWasDoubleClicked:(id)sender {
  NSInteger row = [_tableView clickedRow];
  if (row < 0)
    return;  // Happens e.g. if the table header is double-clicked.
  _tableModel->ActivateTask(_viewToModelMap[row]);
}

- (void)dealloc {
  // Paranoia. These should have been nilled out in -windowWillClose: but let's
  // make sure we have no dangling references.
  [_tableView setDelegate:nil];
  [_tableView setDataSource:nil];
  [super dealloc];
}

// Creates a NSWindow for the task manager and lays out the views inside the
// content view.
- (base::scoped_nsobject<NSWindow>)createAndLayOutWindow {
  static constexpr CGFloat kWindowWidth = 480;
  static constexpr CGFloat kMargin = 20;

  base::scoped_nsobject<NSWindow> window([[NSWindow alloc]
      initWithContentRect:NSMakeRect(195, 240, kWindowWidth, 270)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:YES]);
  [window setMinSize:NSMakeSize(300, 200)];
  [window setTitle:l10n_util::GetNSString(IDS_TASK_MANAGER_TITLE)];

  NSView* contentView = [window contentView];

  // Create the button that terminates the selected process in the table.
  _endProcessButton =
      [ButtonUtils buttonWithTitle:l10n_util::GetNSString(IDS_TASK_MANAGER_KILL)
                            action:@selector(killSelectedProcesses:)
                            target:self];
  [_endProcessButton setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
  [_endProcessButton sizeToFit];
  NSRect buttonFrame = [_endProcessButton frame];
  buttonFrame.size.width += kMargin;
  // Adjust the button's origin so that it is flush with the right-hand side of
  // the table.
  buttonFrame.origin.x =
      NSWidth([contentView frame]) - NSWidth(buttonFrame) - kMargin + 6;
  // Use only half the margin, since the full margin is too much whitespace.
  buttonFrame.origin.y = kMargin / 2;
  [_endProcessButton setFrame:buttonFrame];
  [contentView addSubview:_endProcessButton];

  // Create a scroll view to house the table view.
  CGFloat scrollViewY = NSMaxY(buttonFrame) + kMargin / 2;
  NSRect scrollViewFrame =
      NSMakeRect(kMargin, scrollViewY, kWindowWidth - 2 * kMargin,
                 NSHeight([window frame]) - 2 * kMargin - scrollViewY);
  base::scoped_nsobject<NSScrollView> scrollView(
      [[NSScrollView alloc] initWithFrame:scrollViewFrame]);
  [scrollView setAutoresizesSubviews:YES];
  [scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [scrollView setBorderType:NSBezelBorder];
  [scrollView setFocusRingType:NSFocusRingTypeNone];
  [scrollView setHasVerticalScroller:YES];
  [[scrollView verticalScroller] setControlSize:NSControlSizeSmall];
  [contentView addSubview:scrollView];

  // Create the table view. The data source and delegate are connected in
  // the designated initializer.
  base::scoped_nsobject<NSTableView> tableView(
      [[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, 400, 200)]);
  [tableView setAllowsColumnReordering:NO];
  [tableView setAllowsMultipleSelection:YES];
  [tableView setAutosaveTableColumns:NO];
  [tableView
      setColumnAutoresizingStyle:NSTableViewUniformColumnAutoresizingStyle];
  [tableView setDoubleAction:@selector(tableWasDoubleClicked:)];
  [tableView setFocusRingType:NSFocusRingTypeNone];
  [tableView setIntercellSpacing:NSMakeSize(0, 0)];
  [tableView setUsesAlternatingRowBackgroundColors:YES];
  _tableView = tableView.get();

  [scrollView setDocumentView:tableView];

  return window;
}

// Adds a column which has the given string id as title. |isVisible| specifies
// if the column is initially visible.
- (NSTableColumn*)addColumnWithData:
    (const task_manager::TableColumnData&)columnData {
  base::scoped_nsobject<NSTableColumn> column([[NSTableColumn alloc]
      initWithIdentifier:ColumnIdentifier(columnData.id)]);

  NSTextAlignment textAlignment = (columnData.align == ui::TableColumn::LEFT)
                                      ? NSLeftTextAlignment
                                      : NSRightTextAlignment;

  [[column.get() headerCell]
      setStringValue:l10n_util::GetNSStringWithFixup(columnData.id)];
  [[column.get() headerCell] setAlignment:textAlignment];
  [[column.get() dataCell] setAlignment:textAlignment];

  const CGFloat smallSystemFontSize = [NSFont smallSystemFontSize];
  NSFont* font = nil;
  if (@available(macOS 10.11, *)) {
    font = [NSFont monospacedDigitSystemFontOfSize:smallSystemFontSize
                                            weight:NSFontWeightRegular];
  } else {
    font = [NSFont systemFontOfSize:smallSystemFontSize];
  }
  [[column.get() dataCell] setFont:font];

  [column.get() setHidden:!columnData.default_visibility];
  [column.get() setEditable:NO];

  base::scoped_nsobject<NSSortDescriptor> sortDescriptor(
      [[NSSortDescriptor alloc]
          initWithKey:ColumnIdentifier(columnData.id)
            ascending:columnData.initial_sort_is_ascending]);
  [column.get() setSortDescriptorPrototype:sortDescriptor.get()];

  [column.get() setMinWidth:columnData.min_width];
  int maxWidth = columnData.max_width;
  if (maxWidth < 0)
    maxWidth = 3 * columnData.min_width / 2;  // *1.5 for ints.
  [column.get() setMaxWidth:maxWidth];
  [column.get() setResizingMask:NSTableColumnAutoresizingMask |
                                NSTableColumnUserResizingMask];

  [_tableView addTableColumn:column.get()];
  return column.get();  // Now retained by |tableView_|.
}

// Adds all the task manager's columns to the table.
- (void)setUpTableColumns {
  for (NSTableColumn* column in [_tableView tableColumns])
    [_tableView removeTableColumn:column];

  for (size_t i = 0; i < task_manager::kColumnsSize; ++i) {
    const auto& columnData = task_manager::kColumns[i];
    NSTableColumn* column = [self addColumnWithData:columnData];

    if (columnData.id == IDS_TASK_MANAGER_TASK_COLUMN) {
      // The task column displays an icon for every row, done by an
      // NSButtonCell.
      base::scoped_nsobject<NSButtonCell> nameCell(
          [[NSButtonCell alloc] initTextCell:@""]);
      [nameCell.get() setImagePosition:NSImageLeft];
      [nameCell.get() setButtonType:NSSwitchButton];
      [nameCell.get() setAlignment:[[column dataCell] alignment]];
      [nameCell.get() setFont:[[column dataCell] font]];
      [column setDataCell:nameCell.get()];
    }
  }
}

// Creates a context menu for the table header that allows the user to toggle
// which columns should be shown and which should be hidden (like the Activity
// Monitor.app's table header context menu).
- (void)setUpTableHeaderContextMenu {
  base::scoped_nsobject<NSMenu> contextMenu(
      [[NSMenu alloc] initWithTitle:@"Task Manager context menu"]);
  [contextMenu setDelegate:self];
  [[_tableView headerView] setMenu:contextMenu.get()];
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
  [menu removeAllItems];

  for (NSTableColumn* column in [_tableView tableColumns]) {
    NSMenuItem* item = [menu addItemWithTitle:[[column headerCell] stringValue]
                                       action:@selector(toggleColumn:)
                                keyEquivalent:@""];
    [item setTarget:self];
    [item setRepresentedObject:column];
    [item setState:[column isHidden] ? NSOffState : NSOnState];
  }
}

// Callback for the table header context menu. Toggles visibility of the table
// column associated with the clicked menu item.
- (void)toggleColumn:(NSMenuItem*)item {
  DCHECK([item isKindOfClass:[NSMenuItem class]]);
  if (![item isKindOfClass:[NSMenuItem class]])
    return;

  NSTableColumn* column = [item representedObject];
  int columnId = [[column identifier] intValue];
  DCHECK(column);
  NSInteger oldState = [item state];
  NSInteger newState = oldState == NSOnState ? NSOffState : NSOnState;

  // If hiding the column, make sure at least one column will remain visible.
  if (newState == NSOffState) {
    // Find the first column that will be visible after hiding |column|.
    NSTableColumn* firstRemainingVisibleColumn = nil;

    for (NSTableColumn* nextColumn in [_tableView tableColumns]) {
      if (nextColumn != column && ![nextColumn isHidden]) {
        firstRemainingVisibleColumn = nextColumn;
        break;
      }
    }

    // If no column will be visible, abort the toggle. This will basically cause
    // the toggle operation to silently fail. The other way to ensure at least
    // one visible column is to disable the menu item corresponding to the last
    // remaining visible column. That would place the menu in a weird state to
    // the user, where there's one item somewhere that's grayed out with no
    // clear explanation of why. It will be rare for a user to try hiding all
    // columns, but we still want to guard against it. If they are really intent
    // on hiding the last visible column (perhaps they plan to choose another
    // one after that to be visible), odds are they will try making another
    // column visible and then hiding the one column that would not hide.
    if (firstRemainingVisibleColumn == nil) {
      return;
    }

    // If |column| is being used to sort the table (i.e. it's the primary sort
    // column), make the first remaining visible column the new primary sort
    // column.
    int primarySortColumnId = _currentSortDescriptor.sorted_column_id;
    DCHECK(primarySortColumnId);

    if (primarySortColumnId == columnId) {
      NSSortDescriptor* newSortDescriptor =
          [firstRemainingVisibleColumn sortDescriptorPrototype];
      [_tableView setSortDescriptors:@[ newSortDescriptor ]];
    }
  }

  // Make the change. (This will call back into the SetColumnVisibility()
  // function to actually do the visibility change.)
  _tableModel->ToggleColumnVisibility(columnId);
}

// This function appropriately sets the enabled states on the table's editing
// buttons.
- (void)adjustSelectionAndEndProcessButton {
  bool allSelectionRowsAreKillableTasks = true;
  NSMutableIndexSet* groupIndexes = [NSMutableIndexSet indexSet];

  NSIndexSet* selection = [_tableView selectedRowIndexes];
  for (NSUInteger i = [selection lastIndex];
       i != NSNotFound;
       i = [selection indexLessThanIndex:i]) {
    int modelIndex = _viewToModelMap[i];

    if (!_tableModel->IsTaskKillable(modelIndex))
      allSelectionRowsAreKillableTasks = false;

    int groupStart, groupLength;
    _tableModel->GetRowsGroupRange(modelIndex, &groupStart, &groupLength);
    for (int j = 0; j < groupLength; ++j)
      [groupIndexes addIndex:_modelToViewMap[groupStart + j]];
  }

  [_tableView selectRowIndexes:groupIndexes byExtendingSelection:YES];

  bool enabled = [selection count] > 0 && allSelectionRowsAreKillableTasks &&
                 task_manager::TaskManagerInterface::IsEndProcessEnabled();
  [_endProcessButton setEnabled:enabled];
}

- (void)deselectRows {
  [_tableView deselectAll:self];
}

// Table view delegate methods.

// The selection is being changed by mouse (drag/click).
- (void)tableViewSelectionIsChanging:(NSNotification*)aNotification {
  [self adjustSelectionAndEndProcessButton];
}

// The selection is being changed by keyboard (arrows).
- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  [self adjustSelectionAndEndProcessButton];
}

- (void)windowWillClose:(NSNotification*)notification {
  if (_taskManagerMac) {
    _tableModel->StoreColumnsSettings();
    _taskManagerMac->WindowWasClosed();
    _taskManagerMac = nullptr;
    _tableModel = nullptr;

    // Now that there is no model, ensure that this object gets no data requests
    // in the window of time between the autorelease and the actual dealloc.
    // https://crbug.com/763367
    [_tableView setDelegate:nil];
    [_tableView setDataSource:nil];
  }
  [self autorelease];
}

@end

@implementation TaskManagerWindowController (NSTableDataSource)

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  DCHECK(tableView == _tableView || _tableView == nil);
  return _tableModel->RowCount();
}

- (NSString*)modelTextForRow:(int)row column:(int)columnId {
  DCHECK_LT(static_cast<size_t>(row), _viewToModelMap.size());
  return base::SysUTF16ToNSString(
      _tableModel->GetText(_viewToModelMap[row], columnId));
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  // NSButtonCells expect an on/off state as objectValue. Their title is set
  // in |tableView:dataCellForTableColumn:row:| below.
  if ([[tableColumn identifier] intValue] == IDS_TASK_MANAGER_TASK_COLUMN) {
    return [NSNumber numberWithInt:NSOffState];
  }

  return [self modelTextForRow:rowIndex
                        column:[[tableColumn identifier] intValue]];
}

- (NSCell*)tableView:(NSTableView*)tableView
    dataCellForTableColumn:(NSTableColumn*)tableColumn
                       row:(NSInteger)rowIndex {
  NSCell* cell = [tableColumn dataCellForRow:rowIndex];

  // Set the favicon and title for the task in the name column.
  if ([[tableColumn identifier] intValue] == IDS_TASK_MANAGER_TASK_COLUMN) {
    DCHECK([cell isKindOfClass:[NSButtonCell class]]);
    NSButtonCell* buttonCell = static_cast<NSButtonCell*>(cell);

    NSString* title = [self modelTextForRow:rowIndex
                                    column:[[tableColumn identifier] intValue]];
    NSColor* textColor = [tableView isRowSelected:rowIndex]
                             ? [NSColor alternateSelectedControlTextColor]
                             : [NSColor labelColor];
    NSAttributedString* attributedTitle = [[[NSAttributedString alloc]
        initWithString:title
            attributes:@{
              NSForegroundColorAttributeName : textColor,
              NSFontAttributeName : cell.font
            }] autorelease];
    buttonCell.attributedTitle = attributedTitle;

    buttonCell.image =
        _taskManagerMac->GetImageForRow(_viewToModelMap[rowIndex]);
    buttonCell.refusesFirstResponder = YES;  // Don't push in like a button.
    buttonCell.highlightsBy = NSNoCellMask;
  }

  return cell;
}

- (void)tableView:(NSTableView*)tableView
    sortDescriptorsDidChange:(NSArray*)oldDescriptors {
  if (_withinSortDescriptorsDidChange)
    return;

  NSSortDescriptor* oldDescriptor = [oldDescriptors firstObject];
  NSSortDescriptor* newDescriptor = [[tableView sortDescriptors] firstObject];

  // Implement three-way sorting, toggling "unsorted" as a third option.
  if (oldDescriptor && newDescriptor &&
      [[oldDescriptor key] isEqual:[newDescriptor key]]) {
    // The user clicked to change the sort on the previously sorted column.
    // AppKit toggled the sort order. However, if the sort was toggled to become
    // the initial sorting direction, clear it instead.
    NSTableColumn* column = [tableView
        tableColumnWithIdentifier:ColumnIdentifier(
                                      [[newDescriptor key] intValue])];
    NSSortDescriptor* initialDescriptor = [column sortDescriptorPrototype];
    if ([newDescriptor ascending] == [initialDescriptor ascending]) {
      _withinSortDescriptorsDidChange = YES;
      [_tableView setSortDescriptors:[NSArray array]];
      newDescriptor = nil;
      _withinSortDescriptorsDidChange = NO;
    }
  }

  if (newDescriptor) {
    _currentSortDescriptor.sorted_column_id = [[newDescriptor key] intValue];
    _currentSortDescriptor.is_ascending = [newDescriptor ascending];
  } else {
    _currentSortDescriptor.sorted_column_id = -1;
  }

  [self reloadData];  // Sorts.
}

@end

@implementation TaskManagerWindowController (TestingAPI)

- (NSTableView*)tableViewForTesting {
  return _tableView;
}

- (NSButton*)endProcessButtonForTesting {
  return _endProcessButton;
}

@end

namespace task_manager {

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac implementation:

TaskManagerMac::TaskManagerMac()
    : table_model_(this),
      window_controller_([[TaskManagerWindowController alloc]
          initWithTaskManagerMac:this
                      tableModel:&table_model_]) {
  table_model_.SetObserver(this);  // Hook up the ui::TableModelObserver.
  table_model_.RetrieveSavedColumnsSettingsAndUpdateTable();

  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

// static
TaskManagerMac* TaskManagerMac::instance_ = nullptr;

TaskManagerMac::~TaskManagerMac() {
  table_model_.SetObserver(nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// ui::TableModelObserver implementation:

void TaskManagerMac::OnModelChanged() {
  [window_controller_ deselectRows];
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsChanged(int start, int length) {
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsAdded(int start, int length) {
  [window_controller_ reloadDataWithRows:length addedAtIndex:start];
}

void TaskManagerMac::OnItemsRemoved(int start, int length) {
  [window_controller_ reloadDataWithRows:-length addedAtIndex:start];
}

////////////////////////////////////////////////////////////////////////////////
// TableViewDelegate implementation:

bool TaskManagerMac::IsColumnVisible(int column_id) const {
  return [window_controller_ visibilityOfColumnWithId:column_id];
}

void TaskManagerMac::SetColumnVisibility(int column_id, bool new_visibility) {
  [window_controller_ setColumnWithId:column_id toVisibility:new_visibility];
}

bool TaskManagerMac::IsTableSorted() const {
  return [window_controller_ sortDescriptor].sorted_column_id != -1;
}

TableSortDescriptor TaskManagerMac::GetSortDescriptor() const {
  return [window_controller_ sortDescriptor];
}

void TaskManagerMac::SetSortDescriptor(const TableSortDescriptor& descriptor) {
  [window_controller_ setSortDescriptor:descriptor];
}

////////////////////////////////////////////////////////////////////////////////
// Called by the TaskManagerWindowController:

void TaskManagerMac::WindowWasClosed() {
  delete this;
  instance_ = nullptr;  // |instance_| is static
}

NSImage* TaskManagerMac::GetImageForRow(int row) {
  const NSSize kImageSize = NSMakeSize(16.0, 16.0);
  NSImage* image = gfx::NSImageFromImageSkia(table_model_.GetIcon(row));
  if (image)
    image.size = kImageSize;
  else
    image = [[[NSImage alloc] initWithSize:kImageSize] autorelease];

  return image;
}

void TaskManagerMac::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  Hide();
}

// static
TaskManagerTableModel* TaskManagerMac::Show() {
  if (instance_) {
    [[instance_->window_controller_ window]
        makeKeyAndOrderFront:instance_->window_controller_];
  } else {
    instance_ = new TaskManagerMac();
  }

  return &instance_->table_model_;
}

// static
void TaskManagerMac::Hide() {
  if (instance_)
    [instance_->window_controller_ close];
}

}  // namespace task_manager

namespace chrome {

// Declared in browser_dialogs.h.
task_manager::TaskManagerTableModel* ShowTaskManager(Browser* browser) {
  return task_manager::TaskManagerMac::Show();
}

void HideTaskManager() {
  task_manager::TaskManagerMac::Hide();
}

}  // namespace chrome
