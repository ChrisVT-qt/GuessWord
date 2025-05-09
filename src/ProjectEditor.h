// ProjectEditor.h
// Class definition

#ifndef PROJECTEDITOR_H
#define PROJECTEDITOR_H

// Project includes
#include "AllTaskGroups.h"
#include "AllTaskItems.h"

// Qt includes
#include <QPaintEvent>
#include <QScrollBar>
#include <QWidget>

// Class definition
class ProjectEditor
    : public QWidget
{
    Q_OBJECT



    // ============================================================== Lifecycle
public:
    // Constructor
    ProjectEditor();

    // Destructor
    virtual ~ProjectEditor();

private:
    // Initialize preferences
    static bool Init_Preferences();
    static bool m_PreferencesInitialized;

    void Initialize();

    static const int INVALID_INDEX;



    // ========================================================== Serialization
public:
    // Save
    bool SaveToXML(QDomElement & mrParentElement);

    // Read
    bool ReadFromXML(QDomElement & mrParentElement);



    // =========================================================== GUI: General
private:
    void Initialize_General();

    // Limit for number of lines in content
    int m_MaxLinesInContent;

    // Default font
    QFont m_DefaultFont;

    // Colors
    QColor m_CanvasColor;
    QList < QColor > m_BackgroundColors;
    QColor m_SelectedIndexColor;

    // Opacities
    double m_HoverOpacity;
    double m_SelectedOpacity;

private:
    // Visible IDs
    void UpdateVisibleIDs();
    void UpdateVisibleIDs_Rec(const int mcTaskGroupID, const int mcIndent);
    void ExpandTaskGroup(const int mcTaskGroupID);
    void CollapseTaskGroup(const int mcTaskGroupID);
    QSet < int > m_ExpandedTaskGroups;
    QList < int > m_VisibleIDs;
    QList < AllTaskGroups::ElementType > m_VisibleIDTypes;
    QList < int > m_VisibleIDIndentation;

private slots:
    // Information for a task has changed
    void TaskInformationChanged(const int mcTaskID,
        const AllTaskItems::Information mcInformation);

    // Task has been deleted
    void TaskItemDeleted(const int mcTaskID);

    // Information for a group has changed
    void GroupInformationChanged(const int mcGroupID,
        const AllTaskGroups::Information mcInformation);

    // Members of a group have changed
    void GroupMembersChanged(const int mcGroupID);

    // Group has been deleted
    void TaskGroupDeleted(const int mcGroupID);

    // Schedule has changed
    void ScheduleHasChanged();

    // === Selection
public:
    void SetSelection(const QSet < int > mcNewSelectedTaskIDs,
        const QSet < int > mcNewSelectedGroupIDs);
    void SetSelectedTaskIDs(const QSet < int > mcNewSelectedTaskIDs);
    void SetSelectedGroupIDs(const QSet < int > mcNewSelectedGroupIDs);
    QSet < int > GetSelectedTaskIDs() const;
    QSet < int > GetSelectedGroupIDs() const;
private:
    QSet < int > m_SelectedTaskIDs;
    QSet < int > m_SelectedGroupIDs;

signals:
    void SelectionChanged(const QSet < int > mcSelectedTaskIDs,
        const QSet < int > mcSelectedGroupIDs);

private:
    // Determine which element is at a given position
    int GetIndexAtPosition(const int mcX, const int mcY) const;



    // ======================================================== GUI: Attributes
private:
    void Initialize_Attributes();

    // Attributes
    enum Attributes {
        Attribute_Attachments,
        Attribute_Comments,
        Attribute_CompletionStatus,
        Attribute_CriticalPath,
        Attribute_Duration,
        Attribute_FinishDate,
        Attribute_GanttChart,
        Attribute_ID,
        Attribute_Predecessors,
        Attribute_Resources,
        Attribute_SlackCalendarDays,
        Attribute_SlackWorkdays,
        Attribute_StartDate,
        Attribute_Successors,
        Attribute_Title,
        // Internal
        Attribute_Invalid
    };
    QHash < Attributes, QString > m_AttributeSerializationTitles;

    // Show as...
    enum AttributeDisplayFormat {
        AttributeDisplayFormat_Invalid,
        AttributeDisplayFormat_Default,
        AttributeDisplayFormat_TitleOnly,
        AttributeDisplayFormat_TitleAndParentGroup,
        AttributeDisplayFormat_TitleAndFullHierarchy,
        AttributeDisplayFormat_LongDateOnly,
        AttributeDisplayFormat_LongDateWithWeekday,
        AttributeDisplayFormat_ISODateOnly,
        AttributeDisplayFormat_ISODateWithWeekday,
        AttributeDisplayFormat_YesNo,
        AttributeDisplayFormat_RedGreen,
        AttributeDisplayFormat_ReferencesWithTitle,
        AttributeDisplayFormat_ReferencesOnly,
        AttributeDisplayFormat_CompletionStatusText,
        AttributeDisplayFormat_CompletionStatusPercent,
        AttributeDisplayFormat_CompletionStatusSymbol,
        AttributeDisplayFormat_CommentTitles,
        AttributeDisplayFormat_CommentResponsibilities,
        AttributeDisplayFormat_GanttAutomatic,
        AttributeDisplayFormat_GanttDays,
        AttributeDisplayFormat_GanttWeekdays,
        AttributeDisplayFormat_GanttWeeks,
        AttributeDisplayFormat_GanttMonths,
        AttributeDisplayFormat_GanttYears
    };
    QHash < AttributeDisplayFormat, QString >
        m_AttributeDisplayFormatSerializationTitles;
    QHash < Attributes, QList < AttributeDisplayFormat > >
        m_AttributeAvailableDisplayFormats;

    // Visible attributes
    QList < Attributes > m_VisibleAttributes;
    QHash < Attributes, AttributeDisplayFormat > m_AttributeDisplayFormat;
    QHash < Attributes, QString > m_AttributeContentAlignment;
    QHash < Attributes, QString > m_AttributeGUITitles;
    QHash < AttributeDisplayFormat, QString >
        m_AttributeDisplayFormatGUITitles;

    // Affected attributes when there are changes in task item
    QHash < AllTaskItems::Information, QList < Attributes > >
        m_TaskInformationToAffectedAttributes;
    QHash < AllTaskGroups::Information, QList < Attributes > >
        m_GroupInformationToAffectedAttributes;

    // Content for task attributes
    QPair < QStringList, QList < int > > GetTaskContent(const int mcTaskID,
        const Attributes mcAttribute);
    void UpdateTaskContent_Attachments(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_Comments(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_CompletionStatus(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_CriticalPath(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_Duration(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_FinishDate(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_ID(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_Predecessors(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_Resources(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_SlackCalendarDays(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_SlackWorkdays(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_StartDate(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_Successors(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    void UpdateTaskContent_Title(const int mcTaskID,
        const QHash < AllTaskItems::Information, QString > & mcrTaskInfo);
    QHash < int, QHash < Attributes, QStringList > > m_TaskIDAttributeContent;
    QHash < int, QHash < Attributes, QList < int > > > m_TaskIDAttributeData;

    // Content for task group attributes
    QPair < QStringList, QList < int > > GetGroupContent(const int mcGroupID,
        const Attributes mcAttribute);
    void UpdateGroupContent_CompletionStatus(const int mcGroupID,
        const QHash < AllTaskGroups::Information, QString > & mcrGroupInfo);
    void UpdateGroupContent_Title(const int mcGroupID,
        const QHash < AllTaskGroups::Information, QString > & mcrGroupInfo);
    QHash < int, QHash < Attributes, QStringList > > m_GroupIDAttributeContent;
    QHash < int, QHash < Attributes, QList < int > > > m_GroupIDAttributeData;

    // Dimensions
    QHash < Attributes, int > m_AttributeWidths;
    // Minimum acceptable width
    int m_MinAttributeWidth;
    // Sum of all visible attributes widths
    void CalculateAttributesTotalWidth();
    int m_AttributesTotalWidth;

    // Render dimensions
    // Vertical padding before and after row content
    int m_RowPadding;
    // Horizontal padding before and after attribute content
    int m_AttributePadding;

    // Specifics for tasks/groups
    // Step size for intentation of tasks/groups
    int m_IndentScale;
    // Group expansion triangle: padding above and below
    int m_TrianglePadding;
    // Group expansion triangle: width and height
    int m_TriangleWidth;
    int m_TriangleHeight;
    // Group expansion triangle: Offset between triangle and group name
    int m_TrianglePostOffset;

    // Separator margin (for resizing attributes)
    int m_SeparatorDragMargin;

    // Minimum Header height (attributes)
    int CalculateMinimumHeaderHeight_Attributes();

    // Header image (attributes)
    void UpdateHeaderImage_Attributes();
    QImage m_HeaderImage_Attributes;

    // Row height (attributes)
    int GetRowImageHeight(const int mcIndex);
    int GetRowImageHeight_TaskItem(const int mcIndex);
    QHash < int, int > m_TaskIDToRowImageHeight;
    int GetRowImageHeight_TaskGroup(const int mcIndex);
    QHash < int, int > m_GroupIDToRowImageHeight;

    // Task items images (attributes)
    void CreateRowImage_TaskItem_Attributes(const int mcIndex);
    void CreateRowImage_TaskItem_Attachments(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_Comments(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_CompletionStatus(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_CriticalPath(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_ID(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_Duration(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_FinishDate(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_Predecessors(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_Resources(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_SlackCalendarDays(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_SlackWorkdays(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_StartDate(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_Successors(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskItem_Title(QPainter * mpPainter,
        const int mcIndex);
    QHash < int, QImage > m_TaskItemIDToImage_Attributes;

    // Task group images (attributes)
    void CreateRowImage_TaskGroup_Attributes(const int mcIndex);
    void CreateRowImage_TaskGroup_CompletionStatus(QPainter * mpPainter,
        const int mcIndex);
    void CreateRowImage_TaskGroup_Title(QPainter * mpPainter,
        const int mcIndex);
    QHash < int, QImage > m_TaskGroupIDToImage_Attributes;

    // Get image, regardless of type
    QImage GetRowImage_Attributes(const int mcIndex);

    // Some GUI elements
    int SelectResource() const;
    QString GetName(const QString mcTitle) const;

private slots:
    // Information for a comment has changed
    void CommentChanged(const int mcCommentID);

    // Information for a resource has changed
    void ResourceChanged(const int mcResourceID);

    // Information for an attachment has changed
    void AttachmentChanged(const int mcAttachmentID);



    // ======================================================= GUI: Gantt Chart
private:
    void Initialize_GanttChart();

    // Render dimensions
    int m_GanttChart_BarNorthPadding;
    int m_GanttChart_BarWestPadding;
    int m_GanttChart_BarHeight;
    int m_GanttChart_BarMilestoneWidth;
    int m_GanttChart_HeaderLineHeight;

    // Colors
    QColor m_GanttHolidayBackgroundColor;
    QColor m_TodayColor;
    double m_TodayOpacity;
    QColor m_GanttBarColor;
    QColor m_GanttCriticalPathColor;

public:
    // Start date for Gantt chart
    QDate GetGanttChartStartDate() const;
    void SetGanttChartStartDate(const QDate mcNewStartDate);
    void SetGanttChartStartDateLocked(const bool mcIsLocked);
private:
    QDate m_GanttChart_StartDate;
    bool m_GanttChart_StartDateIsLocked;
signals:
    void GanttChartStartDateChanged(const QDate mcNewStartDate);

public:
    // Scale for Gantt chart
    double GetGanttChartScale() const;
    void SetGanttChartScale(const double mcNewScale);
private:
    double m_GanttChart_Scale;

    // Effective display format for Gantt chart header
    AttributeDisplayFormat GetEffectiveGanttChartDisplayFormat() const;

    void CheckIfCurrentDateChanged();
    QDate m_GanttChart_CurrentDate;

private slots:
    // Minimum Header height (Gantt Chart)
    int CalculateMinimumHeaderHeight_GanttChart();

private:
    // Header image (Gantt chart)
    void UpdateHeaderImage_GanttChart();
    QImage m_HeaderImage_GanttChart;

    // Task item images (Gantt chart)
    void CreateRowImage_TaskItem_GanttChart(const int mcIndex);
    QHash < int, QImage > m_TaskItemIDToImage_GanttChart;

    // Task group images (Gantt chart)
    void CreateRowImage_TaskGroup_GanttChart(const int mcIndex);
    QHash < int, QImage > m_TaskGroupIDToImage_GanttChart;

    // Get image, regardless of type
    QImage GetRowImage_GanttChart(const int mcIndex);

private slots:
    // Calendar (holidays) changed
    void HolidaysChanged();



    // =========================================================== GUI: Drawing
private slots:
    void Initialize_Drawing();

    // Mouse wheel
    virtual void wheelEvent(QWheelEvent * mpEvent);
signals:
    void TopLeftChanged();

private slots:
    // Redraw
    void paintEvent(QPaintEvent * mpEvent);
private:
    // Header height
    void UpdateHeaderHeight();
    int m_HeaderHeight;

public:
    // Positions
    int GetTopOffset();
    int GetLeftOffset() const;

    // Maximum offsets
    int GetMaximumTopOffset();
    int GetMaximumLeftOffset() const;

    // Scroll to position
    void SetTopOffset(const int mcNewTopOffset);
    void SetLeftOffset(const int mcNewLeftOffset);
private:
    // Top
    int m_TopIndex;
    int m_TopOffset;
    int m_LeftOffset;

    // Attributes shown
    QList < int > m_VisibleAttributesLeftCoordinates;
    QList < int > m_VisibleAttributesRightCoordinates;

    // Rows shown
    QList < int > m_VisibleIDTopCoordinates;
    QList < int > m_VisibleIDBottomCoordinates;

protected:
    // Moving the mouse
    virtual void mouseMoveEvent(QMouseEvent * mpEvent);

    // Clicked
    virtual void mousePressEvent(QMouseEvent * mpEvent);
    void mousePressEvent_Header(const int mcX, const int mcY);
    void mousePressEvent_Content(const int mcX, const int mcY);
    void mousePressEvent_Content_DeselectAll();
    void mousePressEvent_Content_SelectSingleItem(const int mcIndex);
    void mousePressEvent_Content_SelectItemRange(const int mcIndex);
    void mousePressEvent_Content_ToggleSelectedItem(const int mcIndex);
    int m_SelectRange_AnchorIndex;

    // Drag and drop
    virtual void mouseReleaseEvent(QMouseEvent * mpEvent);

    // Double-clicked
    virtual void mouseDoubleClickEvent(QMouseEvent * mpEvent);

private:
    // == Everything with "hovering"

    // Cell actions
    enum CellActions
    {
        CellAction_Add,
        CellAction_Subtract,
        CellAction_Edit,
        CellAction_NotStarted,
        CellAction_Started,
        CellAction_Completed,
        CellAction_Invalid
    };
    QHash < CellActions, QString > m_CellActionTitles;

    // Hovering
    void Hover(const int mcX, const int mcY);
    void Hover_Header(const int mcX, const int mcY);
    void Hover_Header_GanttChart(const int mcX, const int mcY);
    void Hover_Content(const int mcX, const int mcY);
    void Hover_Content_Row(const int mcX, const int mcY);

    // Active cell actions
    QList < int > m_HoveredCell_ActionXMin;
    QList < int > m_HoveredCell_ActionXMax;
    QList < int > m_HoveredCell_ActionYMin;
    QList < int > m_HoveredCell_ActionYMax;
    QList < CellActions > m_HoveredCell_ActionType;
    QList < int > m_HoveredCell_ActionData;

    // Currently hovered
    int m_HoveredIndex;
    int m_HoveredID;
    int m_HoveredCell_X;
    int m_HoveredCell_Y;
    AllTaskGroups::ElementType m_HoveredIDType;
    Attributes m_HoveredAttribute;
    CellActions m_HoveredCellAction;

    // Available cell action images
    QImage m_ImagePlus;
    QImage m_ImageMinus;
    QImage m_ImageEdit;
    QImage m_ImageGreen;
    QImage m_ImageYellow;
    QImage m_ImageRed;

    // Executing cell actions
    void ExecuteCellAction();
    void ExecuteCellAction_ID();
    void ExecuteCellAction_Title();
    void ExecuteCellAction_Duration();
    void ExecuteCellAction_Predecessors();
    void ExecuteCellAction_Successors();
    void ExecuteCellAction_CompletionStatus();
    void ExecuteCellAction_Resources();
    void ExecuteCellAction_Attachments();
    void ExecuteCellAction_Comments();

private:
    // == Everything with "dragging"

    // Dragging
    void Drag(const int mcX, const int mcY);
    void Drag_Header(const int mcX, const int mcY);
    void Drag_Header_Separator(const int mcX, const int mcY);

    QPoint m_DragStartPosition;
    Attributes m_DragAttribute;
    Attributes m_DragAttributeWidth_Attribute;
    int m_DragAttributeWidth_OriginalWidth;
    bool m_IsLeftMouseButtonPressed;
    bool m_IsDragging;

private:
    // Determine which attribute is at a given position
    Attributes GetAttributeAtPosition(const int mcX, const int mcY) const;

signals:
    // Show message in Main Window
    void ShowMessage(const QString mcMessage, const bool mcIsWarning);

protected:
    // Resizing
    virtual void resizeEvent(QResizeEvent * mpEvent);
signals:
    void SizeChanged();



    // =========================================================== Context Menu
private slots:
    // Context menu
    virtual void contextMenuEvent(QContextMenuEvent * mpEvent);

    // Header
    void ContextMenu_Header(const QPoint mcPosition);
    void ContextMenu_ToggleVisibility(
        const ProjectEditor::Attributes mcAttribute,
        const ProjectEditor::Attributes mcAnchorAttribute);
    void ContextMenu_SetDisplayFormat(
        const ProjectEditor::Attributes mcAttribute,
        const ProjectEditor::AttributeDisplayFormat mcNewDisplayFormat);

    // Content
    void ContextMenu_Content(const QPoint mcPosition);
    void ContextMenu_Selection(const QPoint mcPosition);
    void ContextMenu_Task(const QPoint mcPosition);
    void ContextMenu_Group(const QPoint mcPosition);
    void ContextMenu_Canvas(const QPoint mcPosition);

    // Task items
    void Context_EditTask(const int mcTaskID);
    void Context_SetCompletionStatus(const int mcTaskID,
        const QString mcNewStatus, const QString mcTimeliness);
    void Context_ToggleResource(const int mcTaskID, const int mcResourceID);
    void Context_ToggleResource(const QSet < int > mcTaskIDs,
        const int mcResourceID);
    void Context_AddResource(const int mcTaskID);
    void Context_AddResource(const QSet < int > mcTaskIDs);
    void Context_AddComment(const int mcTaskID);
    void Context_ShowAttachment(const int mcAttachmentID);
    void Context_AddAttachment(const int mcTaskID);
    void Context_DeleteTask(const int mcTaskID);
    void Context_DeleteTasks(const QSet < int > mcTaskID);
    void Context_AddTaskAfterTask(const int mcAnchroTaskID);
    void Context_AddGroupAfterTask(const int mcAnchorTaskID);

    // Group items
    void Context_EditGroup(const int mcGroupID);
    void Context_ExpandGroup(const int mcGroupID);
    void Context_CollapseGroup(const int mcGroupID);
    void Context_DeleteGroup(const int mcGroupID);
    void Context_AddTaskInGroup(const int mcParentGroupID);
    void Context_NewGroupInGroup(const int mcParentGroupID);

    // Selection
    void Context_DeleteSelection();



    // ================================================================== Debug
public:
    // Dump everything
    void Dump() const;
};

#endif
