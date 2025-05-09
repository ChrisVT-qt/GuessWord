// ProjectEditor.cpp
// Class implementation

// Project includes
#include "AllAttachments.h"
#include "AllComments.h"
#include "AllResources.h"
#include "AllTaskGroups.h"
#include "AllTaskItems.h"
#include "AllTaskLinks.h"
#include "AutocompletionLineEdit.h"
#include "Calendar.h"
#include "CallTracer.h"
#include "CommentEditor.h"
#include "GroupEditor.h"
#include "LinkEditor.h"
#include "MessageLogger.h"
#include "Preferences.h"
#include "Project.h"
#include "ProjectEditor.h"
#include "StringHelper.h"
#include "TaskEditor.h"

// Qt includes
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>
#include <QTextEdit>



// ================================================================== Lifecycle



///////////////////////////////////////////////////////////////////////////////
// Constructor
ProjectEditor::ProjectEditor()
{
    CALL_IN("");

    // Initialize some stuff
    Initialize();

    // Track mouse motion
    setMouseTracking(true);

    // Connect some signals
    AllTaskItems * at = AllTaskItems::Instance();
    connect (at,
        SIGNAL(InformationChanged(const int, const AllTaskItems::Information)),
        this,
        SLOT(TaskInformationChanged(const int,
            const AllTaskItems::Information)));
    connect (at, SIGNAL(Deleted(const int)),
        this, SLOT(TaskItemDeleted(const int)));

    AllTaskGroups * ag = AllTaskGroups::Instance();
    connect (ag, SIGNAL(GroupInformationChanged(const int,
            const AllTaskGroups::Information)),
        this, SLOT(GroupInformationChanged(const int,
            const AllTaskGroups::Information)));
    connect (ag, SIGNAL(GroupMembersChanged(const int)),
        this, SLOT(GroupMembersChanged(const int)));
    connect (ag, SIGNAL(Deleted(const int)),
        this, SLOT(TaskGroupDeleted(const int)));

    AllComments * ac = AllComments::Instance();
    connect (ac, SIGNAL(Changed(const int)),
        this, SLOT(CommentChanged(const int)));

    AllResources * ar = AllResources::Instance();
    connect (ar, SIGNAL(Changed(const int)),
        this, SLOT(ResourceChanged(const int)));

    AllAttachments * aa = AllAttachments::Instance();
    connect (aa, SIGNAL(Changed(const int)),
        this, SLOT(AttachmentChanged(const int)));

    Calendar * c = Calendar::Instance();
    connect (c, SIGNAL(HolidaysChanged()),
        this, SLOT(HolidaysChanged()));

    Project * p = Project::Instance();
    connect (p, SIGNAL(ScheduleInvalid()),
        this, SLOT(ScheduleHasChanged()));
    connect (p, SIGNAL(ScheduleUpdated()),
        this, SLOT(ScheduleHasChanged()));

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Destructor
ProjectEditor::~ProjectEditor()
{
    CALL_IN("");

    // Nothing to do

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Initialize preferences
bool ProjectEditor::Init_Preferences()
{
    CALL_IN("");

    // Initialize preferences
    Preferences * pr = Preferences::Instance();
    pr -> AddValidTag("GUI:Expand new groups");
    pr -> SetValue("GUI:Expand new groups", "yes");

    CALL_OUT("");
    return true;
}



///////////////////////////////////////////////////////////////////////////////
// Initialize preferences
bool ProjectEditor::m_PreferencesInitialized =
    ProjectEditor::Init_Preferences();



///////////////////////////////////////////////////////////////////////////////
// Initialize internal data stucture
void ProjectEditor::Initialize()
{
    CALL_IN("");

    Initialize_General();
    Initialize_Attributes();
    Initialize_GanttChart();
    Initialize_Drawing();

    // Background
    QPalette mypalette = palette();
    mypalette.setColor(QPalette::Window, m_CanvasColor);
    setPalette(mypalette);
    setAutoFillBackground(true);

    // New selection requires other classes to know
    emit SelectionChanged(QSet < int >(), QSet < int >());

    CALL_OUT("");
}


///////////////////////////////////////////////////////////////////////////////
// Invalid index
const int ProjectEditor::INVALID_INDEX = -1;



// ============================================================== Serialization



///////////////////////////////////////////////////////////////////////////////
// Save (DOM)
bool ProjectEditor::SaveToXML(QDomElement & mrParentElement)
{
    CALL_IN("mrParentElement=...");

    // <editor>
    //  <columns>
    //   <column visible="no" type="critical path" format="red/green"
    //    title="Critical" align="center" width="70"/>
    //   ...
    //  </columns>
    //  <expanded_groups>
    //   <group id="3"/>
    //   ...
    //  </expanded_groups>
    //  <visible top_offset="0" left_offset="0" top_index="-1"/>
    //  <gantt scale="20" units="automatic"/>
    // </editor>

    // Valid parent?
    if (mrParentElement.isNull())
    {
        const QString reason = tr("Invalid parent element.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return false;
    }

    // <editor>
    QDomDocument dom_doc = mrParentElement.ownerDocument();
    QDomElement dom_editor = dom_doc.createElement("editor");
    mrParentElement.appendChild(dom_editor);

    // Attribute settings
    QDomElement dom_columns = dom_doc.createElement("columns");
    dom_editor.appendChild(dom_columns);
    const QList < Attributes > all_attributes =
        m_AttributeSerializationTitles.keys();
    for (const Attributes attribute : all_attributes)
    {
        QDomElement dom_column = dom_doc.createElement("column");
        dom_columns.appendChild(dom_column);

        // Type
        dom_column.setAttribute("type",
            m_AttributeSerializationTitles[attribute]);

        if (attribute == Attribute_GanttChart)
        {
            // Start date is not saved; it's always set to "today" when
            // starting the application.
            dom_column.setAttribute("scale", m_GanttChart_Scale);
        } else
        {
            // Title
            dom_column.setAttribute("title", m_AttributeGUITitles[attribute]);

            // Alignment
            dom_column.setAttribute("align",
                m_AttributeContentAlignment[attribute]);

            // Attribute width
            dom_column.setAttribute("width", m_AttributeWidths[attribute]);
        }

        // Visibility and order
        dom_column.setAttribute("visible",
            m_VisibleAttributes.contains(attribute) ? "yes" : "no");
        if (m_VisibleAttributes.contains(attribute))
        {
            dom_column.setAttribute("index",
                m_VisibleAttributes.indexOf(attribute));
        }

        // Display format
        dom_column.setAttribute("format",
            m_AttributeDisplayFormatSerializationTitles[
                m_AttributeDisplayFormat[attribute]]);
    }

    // Expanded groups
    QDomElement dom_groups = dom_doc.createElement("expanded_groups");
    dom_editor.appendChild(dom_groups);
    for (const int group_id : qAsConst(m_ExpandedTaskGroups))
    {
        QDomElement dom_group = dom_doc.createElement("group");
        dom_groups.appendChild(dom_group);
        dom_group.setAttribute("id", group_id);
    }

    // Top index and offset
    QDomElement dom_visible = dom_doc.createElement("visible");
    dom_editor.appendChild(dom_visible);
    dom_visible.setAttribute("top_index", m_TopIndex);
    dom_visible.setAttribute("top_offset", m_TopOffset);
    dom_visible.setAttribute("left_offset", m_LeftOffset);

    // All good
    CALL_OUT("");
    return true;
}



///////////////////////////////////////////////////////////////////////////////
// Read (DOM)
bool ProjectEditor::ReadFromXML(QDomElement & mrParentElement)
{
    CALL_IN("mrParentElement=...");

    // Valid parent?
    if (mrParentElement.isNull())
    {
        const QString reason = tr("Invalid parent element.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return false;
    }

    // Reset internal data structure
    Initialize();

    // <editor>
    QDomElement dom_editor = mrParentElement.firstChildElement("editor");

    // Column settings
    QHash < int, Attributes > attribute_order;
    QDomElement dom_columns = dom_editor.firstChildElement("columns");
    for (QDomElement dom_column = dom_columns.firstChildElement("column");
         !dom_column.isNull();
         dom_column = dom_column.nextSiblingElement("column"))
    {
        // Attribute
        const QString type_text = dom_column.attribute("type");
        const Attributes attribute =
            m_AttributeSerializationTitles.key(type_text, Attribute_Invalid);
        if (attribute == Attribute_Invalid)
        {
            const QString reason = tr("Unknown column type \"%1\".")
                .arg(type_text);
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return false;
        }

        if (attribute == Attribute_GanttChart)
        {
            // Scale
            m_GanttChart_Scale = dom_column.attribute("scale").toDouble();
        } else
        {
            // Title
            m_AttributeSerializationTitles[attribute] =
                dom_column.attribute("title");

            // Alignment
            m_AttributeContentAlignment[attribute] =
                dom_column.attribute("align");

            // Attribute width
            m_AttributeWidths[attribute] =
                dom_column.attribute("width").toInt();
        }

        // Visibility and order
        if (dom_column.attribute("visible") == "yes")
        {
            const int index = dom_column.attribute("index").toInt();
            attribute_order[index] = attribute;
        }

        // Display format
        const QString format_text = dom_column.attribute("format");
        const AttributeDisplayFormat format =
            m_AttributeDisplayFormatSerializationTitles.key(format_text,
                AttributeDisplayFormat_Invalid);
        if (format == AttributeDisplayFormat_Invalid)
        {
            const QString reason = tr("Invalid column display format \"%1\".")
                .arg(format_text);
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return false;
        }
        m_AttributeDisplayFormat[attribute] = format;
    }

    // Order of visible attributes
    QList < int > sorted_indices = attribute_order.keys();
    std::sort(sorted_indices.begin(), sorted_indices.end());
    m_VisibleAttributes.clear();
    for (const int index : qAsConst(sorted_indices))
    {
        m_VisibleAttributes << attribute_order[index];
    }

    // Calculate total width of visible attributes
    CalculateAttributesTotalWidth();

    // Expanded groups
    QDomElement dom_groups = dom_editor.firstChildElement("expanded_groups");
    for (QDomElement dom_group = dom_groups.firstChildElement("group");
         !dom_group.isNull();
         dom_group = dom_group.nextSiblingElement("group"))
    {
        m_ExpandedTaskGroups << dom_group.attribute("id").toInt();
    }

    // Top index and offset
    QDomElement dom_visible = dom_editor.firstChildElement("visible");
    m_TopIndex = dom_visible.attribute("top_index").toInt();
    m_TopOffset = dom_visible.attribute("top_offset").toInt();
    m_LeftOffset = dom_visible.attribute("left_offset", "0").toInt();

    // All good
    CALL_OUT("");
    return true;
}



// =============================================================== GUI: General



///////////////////////////////////////////////////////////////////////////////
// Initialize all general pieces of information
void ProjectEditor::Initialize_General()
{
    CALL_IN("");

    // Maxmimum visible lines in content
    m_MaxLinesInContent = 5;

    // Default font
    m_DefaultFont = font();
    m_DefaultFont.setPixelSize(14);

    // Colors
    m_CanvasColor = QColor(240,240,240);
    m_BackgroundColors.clear();
    m_BackgroundColors << QColor(230, 230, 230) << QColor(240, 240, 240);
    m_SelectedIndexColor = QColor(160,160,255);

    // Opacities
    m_HoverOpacity = 0.1;
    m_SelectedOpacity = 0.5;

    // Root group is always visible
    m_ExpandedTaskGroups.clear();
    m_ExpandedTaskGroups << AllTaskGroups::ROOT_ID;
    m_VisibleIDs.clear();
    m_VisibleIDTypes.clear();
    m_VisibleIDIndentation.clear();

    // Nothing selected
    m_SelectedTaskIDs.clear();
    m_SelectedGroupIDs.clear();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Visible IDs
void ProjectEditor::UpdateVisibleIDs()
{
    CALL_IN("");

    // Reset
    m_VisibleIDs.clear();
    m_VisibleIDTypes.clear();
    m_VisibleIDIndentation.clear();

    // Loop groups
    UpdateVisibleIDs_Rec(AllTaskGroups::ROOT_ID, 0);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Visible IDs (recursive part)
void ProjectEditor::UpdateVisibleIDs_Rec(const int mcTaskGroupID,
    const int mcIndent)
{
    CALL_IN(QString("mcTaskGroupID=%1, mcIndent=%2")
        .arg(QString::number(mcTaskGroupID),
             QString::number(mcIndent)));

    // Private method - no checks

    // Get group members
    AllTaskGroups * at = AllTaskGroups::Instance();
    const QPair < QList < int >, QList < AllTaskGroups::ElementType > >
        new_elements = at -> GetElementIDs(mcTaskGroupID);

    // Add in order
    for (int index = 0;
         index < new_elements.first.size();
         index++)
    {
        // Abbreviation
        const int element_id = new_elements.first[index];
        const AllTaskGroups::ElementType element_type =
            new_elements.second[index];

        // Add to list of visible IDs
        m_VisibleIDs << element_id;
        m_VisibleIDTypes << element_type;
        m_VisibleIDIndentation << mcIndent;

        // Branch into expanded groups
        if (element_type == AllTaskGroups::ElementType_GroupID &&
            m_ExpandedTaskGroups.contains(element_id))
        {
            UpdateVisibleIDs_Rec(element_id, mcIndent+1);
        }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Show contents of a task group
void ProjectEditor::ExpandTaskGroup(const int mcTaskGroupID)
{
    CALL_IN(QString("mcTaskGroupID=%1")
        .arg(QString::number(mcTaskGroupID)));

    // Private method - no checks

    // Check if task group is already expanded
    if (m_ExpandedTaskGroups.contains(mcTaskGroupID))
    {
        // Error
        const QString reason = tr("Group %1 already is expanded.")
            .arg(QString::number(mcTaskGroupID));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Expand group
    m_ExpandedTaskGroups += mcTaskGroupID;

    // Needs to be recreated
    m_TaskGroupIDToImage_Attributes.remove(mcTaskGroupID);
    m_GroupIDToRowImageHeight.remove(mcTaskGroupID);

    // Redo visible IDs
    UpdateVisibleIDs();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Hide contents of a task group
void ProjectEditor::CollapseTaskGroup(const int mcTaskGroupID)
{
    CALL_IN(QString("mcTaskGroupID=%1")
        .arg(QString::number(mcTaskGroupID)));

    // Private method - no checks

    // Check if task group is expanded
    if (!m_ExpandedTaskGroups.contains(mcTaskGroupID))
    {
        // Error
        const QString reason = tr("Group %1 already is collapsed.")
            .arg(QString::number(mcTaskGroupID));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Collapse group
    m_ExpandedTaskGroups -= mcTaskGroupID;

    // Needs to be recreated
    m_TaskGroupIDToImage_Attributes.remove(mcTaskGroupID);
    m_GroupIDToRowImageHeight.remove(mcTaskGroupID);

    // Redo visible IDs
    UpdateVisibleIDs();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Information for a task has changed
void ProjectEditor::TaskInformationChanged(const int mcTaskID,
    const AllTaskItems::Information mcInformation)
{
    CALL_IN(QString("mcTaskID=%1, mcInformation=...")
        .arg(QString::number(mcTaskID)));

    // Check if task ID is valid
    AllTaskItems * at = AllTaskItems::Instance();
    if (!at -> DoesIDExist(mcTaskID))
    {
        const QString reason = tr("Task ID %1 does not exist.")
            .arg(QString::number(mcTaskID));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Check if we know this information
    if (!m_TaskInformationToAffectedAttributes.contains(mcInformation))
    {
        const QString reason = tr("Unknown task information.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Affected tasks
    QList < int > affected_task_ids;

    // Remove cached content
    bool is_visible_change = false;
    for (const Attributes attribute :
        m_TaskInformationToAffectedAttributes[mcInformation])
    {
        m_TaskIDAttributeContent[mcTaskID].remove(attribute);
        is_visible_change = is_visible_change
            || m_VisibleAttributes.contains(attribute);
    }
    if (is_visible_change)
    {
        affected_task_ids << mcTaskID;
    }

    // Remove collateral
    if (mcInformation == AllTaskItems::Information_AnyInformation ||
        mcInformation == AllTaskItems::Information_Reference ||
        mcInformation == AllTaskItems::Information_Title)
    {
        // Need to update tasks linking to and from current task ID
        AllTaskLinks * al = AllTaskLinks::Instance();
        QList < int > affected_task_ids;
        if (m_VisibleAttributes.contains(Attribute_Predecessors))
        {
            for (int task_id : al -> GetSuccessorTaskIDsForTaskID(mcTaskID))
            {
                m_TaskIDAttributeContent[task_id]
                    .remove(Attribute_Predecessors);
                affected_task_ids << task_id;
            }
        }
        if (m_VisibleAttributes.contains(Attribute_Successors))
        {
            for (int task_id : al -> GetPredecessorTaskIDsForTaskID(mcTaskID))
            {
                m_TaskIDAttributeContent[task_id]
                    .remove(Attribute_Successors);
                affected_task_ids << task_id;
            }
        }
    }
    if (mcInformation == AllTaskItems::Information_LinkedTasks)
    {
        // The critical path may change, so we need to redo the entire
        // rendering of the Gantt chart
        m_TaskItemIDToImage_GanttChart.clear();
    }

    // Remove cached images and row height
    bool update_required = false;
    for (int task_id : affected_task_ids)
    {
        if (m_TaskItemIDToImage_Attributes.contains(task_id) ||
            m_TaskItemIDToImage_GanttChart.contains(task_id))
        {
            m_TaskItemIDToImage_Attributes.remove(task_id);
            m_TaskItemIDToImage_GanttChart.remove(task_id);
            m_TaskIDToRowImageHeight.remove(task_id);
            update_required = true;
        }
    }
    if (update_required)
    {
        update();
    }

    // !!! If we have filters, visibility of the task may change

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task has been deleted
void ProjectEditor::TaskItemDeleted(const int mcTaskID)
{
    CALL_IN(QString("mcTaskID=%1")
        .arg(QString::number(mcTaskID)));

    m_TaskItemIDToImage_Attributes.remove(mcTaskID);
    m_TaskItemIDToImage_GanttChart.remove(mcTaskID);
    m_TaskIDToRowImageHeight.remove(mcTaskID);
    UpdateVisibleIDs();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Information for a group has changed
void ProjectEditor::GroupInformationChanged(const int mcGroupID,
    const AllTaskGroups::Information mcInformation)
{
    CALL_IN(QString("mcGroupID=%1, mcInformation=...")
        .arg(QString::number(mcGroupID)));

    // Check if group ID is valid
    AllTaskGroups * ag = AllTaskGroups::Instance();
    if (!ag -> DoesIDExist(mcGroupID))
    {
        const QString reason = tr("Task group ID %1 does not exist.")
            .arg(QString::number(mcGroupID));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Check if we know this information
    if (!m_GroupInformationToAffectedAttributes.contains(mcInformation))
    {
        const QString reason = tr("Unknown task group information.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Affected elements
    QList < int > affected_group_ids;
    QList < int > affected_task_ids;

    // Remove cached content
    bool is_visible_change = false;
    for (const Attributes attribute :
        m_GroupInformationToAffectedAttributes[mcInformation])
    {
        m_GroupIDAttributeContent[mcGroupID].remove(attribute);
        is_visible_change = is_visible_change
            || m_VisibleAttributes.contains(attribute);
    }
    if (is_visible_change)
    {
        affected_group_ids << mcGroupID;
    }

    // Remove collateral
    if (mcInformation == AllTaskGroups::Information_Title &&
        m_VisibleAttributes.contains(Attribute_Title))
    {
        // Check if are showing parents
        if (m_AttributeDisplayFormat[Attribute_Title] ==
            AttributeDisplayFormat_TitleAndParentGroup)
        {
            // Parents only - only direct child elements (tasks and groups)
            // affected
            const QPair < QList < int >, QList < AllTaskGroups::ElementType > >
                child_elements = ag -> GetElementIDs(mcGroupID);
            const QList < int > & child_ids = child_elements.first;
            const QList < AllTaskGroups::ElementType > & child_types =
                child_elements.second;
            for (int child_index = 0;
                 child_index < child_ids.size();
                 child_index++)
            {
                switch (child_types[child_index])
                {
                case AllTaskGroups::ElementType_TaskID:
                    m_TaskIDAttributeContent[child_ids[child_index]]
                        .remove(Attribute_Title);
                    affected_task_ids << child_ids[child_index];
                    break;

                case AllTaskGroups::ElementType_GroupID:
                    m_GroupIDAttributeContent[child_ids[child_index]]
                        .remove(Attribute_Title);
                    affected_group_ids << child_ids[child_index];
                    break;

                default:
                    // Error
                    const QString reason = tr("Unknown element type");
                    MessageLogger::Error(CALL_METHOD,
                        reason);
                    CALL_OUT(reason);
                    return;
                }
            }
        } else if (m_AttributeDisplayFormat[Attribute_Title] ==
            AttributeDisplayFormat_TitleAndFullHierarchy)
        {
            // Full hierarchy - all child elements (tasks and groups) affected
            QList < int > remaining_group_ids;
            remaining_group_ids << mcGroupID;
            while (!remaining_group_ids.isEmpty())
            {
                const int group_id = remaining_group_ids.takeFirst();
                const QPair < QList < int >,
                    QList < AllTaskGroups::ElementType > > child_elements =
                        ag -> GetElementIDs(group_id);
                const QList < int > & child_ids = child_elements.first;
                const QList < AllTaskGroups::ElementType > & child_types =
                    child_elements.second;
                for (int child_index = 0;
                     child_index < child_ids.size();
                     child_index++)
                {
                    switch (child_types[child_index])
                    {
                    case AllTaskGroups::ElementType_TaskID:
                        m_TaskIDAttributeContent[child_ids[child_index]]
                            .remove(Attribute_Title);
                        affected_task_ids << child_ids[child_index];
                        break;

                    case AllTaskGroups::ElementType_GroupID:
                        m_GroupIDAttributeContent[child_ids[child_index]]
                            .remove(Attribute_Title);
                        affected_group_ids << child_ids[child_index];
                        remaining_group_ids << child_ids[child_index];
                        break;

                    default:
                        // Error
                        const QString reason = tr("Unknown element type");
                        MessageLogger::Error(CALL_METHOD,
                            reason);
                        CALL_OUT(reason);
                        return;
                    }
                }
            }
        }
    }

    // Remove cached images and row height
    bool update_required = false;
    for (int group_id : affected_group_ids)
    {
        if (m_TaskGroupIDToImage_Attributes.contains(group_id) ||
            m_TaskGroupIDToImage_GanttChart.contains(group_id))
        {
            m_TaskGroupIDToImage_Attributes.remove(group_id);
            m_TaskGroupIDToImage_GanttChart.remove(group_id);
            m_GroupIDToRowImageHeight.remove(group_id);
            update_required = true;
        }
    }
    for (int task_id : affected_task_ids)
    {
        if (m_TaskItemIDToImage_Attributes.contains(task_id) ||
            m_TaskItemIDToImage_GanttChart.contains(task_id))
        {
            m_TaskItemIDToImage_Attributes.remove(task_id);
            m_TaskItemIDToImage_GanttChart.remove(task_id);
            m_TaskIDToRowImageHeight.remove(task_id);
            update_required = true;
        }
    }
    if (update_required)
    {
        update();
    }

    // !!! If we have filters, visibility of the task may change

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Members of a group have changed
void ProjectEditor::GroupMembersChanged(const int mcGroupID)
{
    CALL_IN(QString("mcGroupID=%1")
        .arg(QString::number(mcGroupID)));

    // Elements may or may not be visible
    UpdateVisibleIDs();

    // Rebuild information from this group down
    QList < int > group_ids;
    group_ids << mcGroupID;
    AllTaskGroups * ag = AllTaskGroups::Instance();
    while (!group_ids.isEmpty())
    {
        const int group_id = group_ids.takeFirst();
        m_TaskGroupIDToImage_Attributes.remove(group_id);
        m_TaskGroupIDToImage_GanttChart.remove(group_id);
        m_GroupIDToRowImageHeight.remove(group_id);

        // Branch into child elements
        const QPair < QList < int >, QList < AllTaskGroups::ElementType > >
            members = ag -> GetElementIDs(group_id);
        for (int index = 0;
             index < members.first.size();
             index++)
        {
            const int element_id = members.first[index];
            const AllTaskGroups::ElementType element_type =
                members.second[index];
            if (element_type == AllTaskGroups::ElementType_GroupID)
            {
                group_ids << element_id;
            } else
            {
                m_TaskItemIDToImage_Attributes.remove(element_id);
                m_TaskIDToRowImageHeight.remove(element_id);
            }
        }
    }

    // Update visuals
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Group has been deleted
void ProjectEditor::TaskGroupDeleted(const int mcGroupID)
{
    CALL_IN(QString("mcGroupID=%1")
        .arg(QString::number(mcGroupID)));

    m_TaskGroupIDToImage_Attributes.remove(mcGroupID);
    m_TaskGroupIDToImage_GanttChart.remove(mcGroupID);
    m_ExpandedTaskGroups.remove(mcGroupID);
    UpdateVisibleIDs();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Schedule has changed
void ProjectEditor::ScheduleHasChanged()
{
    CALL_IN("");

    // Get affected task items
    Project * p = Project::Instance();
    const QList < int > task_ids = p -> GetAffectedTaskIDs();
    for (const int task_id : task_ids)
    {
        m_TaskItemIDToImage_Attributes.remove(task_id);
        m_TaskItemIDToImage_GanttChart.remove(task_id);
        m_TaskIDToRowImageHeight.remove(task_id);
    }

    // Update visible IDs
    UpdateVisibleIDs();

    // Update visuals
    update();

    CALL_OUT("");
}



// === Selection



///////////////////////////////////////////////////////////////////////////////
// Set selection
void ProjectEditor::SetSelection(const QSet < int > mcNewSelectedTaskIDs,
    const QSet < int > mcNewSelectedGroupIDs)
{
    QList < QString > all_selected_task_ids;
    for (const int task_id : mcNewSelectedTaskIDs)
    {
        all_selected_task_ids << QString::number(task_id);
    }
    std::sort(all_selected_task_ids.begin(), all_selected_task_ids.end());
    QList < QString > all_selected_group_ids;
    for (const int group_id : mcNewSelectedGroupIDs)
    {
        all_selected_group_ids << QString::number(group_id);
    }
    std::sort(all_selected_group_ids.begin(), all_selected_group_ids.end());
    CALL_IN(QString("mcNewSelectedTaskIDs={%1}, mcNewSelectedGroupIDs={%2}")
        .arg(all_selected_task_ids.join(", "),
             all_selected_group_ids.join(", ")));

    // Check if all task IDs exist
    AllTaskItems * at = AllTaskItems::Instance();
    for (const int task_id : mcNewSelectedTaskIDs)
    {
        if (!at -> DoesIDExist(task_id))
        {
            const QString reason = tr("Task ID %1 does not exist.")
                .arg(QString::number(task_id));
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return;
        }
    }

    // Check if all group IDs exist
    AllTaskGroups * ag = AllTaskGroups::Instance();
    for (const int group_id : mcNewSelectedGroupIDs)
    {
        if (!ag -> DoesIDExist(group_id))
        {
            const QString reason = tr("Group ID %1 does not exist.")
                .arg(QString::number(group_id));
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return;
        }
    }

    // Need later
    bool selection_changed = false;

    // Unselect items
    for (const int task_id : qAsConst(m_SelectedTaskIDs))
    {
        if (!mcNewSelectedTaskIDs.contains(task_id))
        {
            m_TaskItemIDToImage_Attributes.remove(task_id);
            m_TaskItemIDToImage_GanttChart.remove(task_id);
            m_TaskIDToRowImageHeight.remove(task_id);
            selection_changed = true;
        }
    }
    for (const int group_id : qAsConst(m_SelectedGroupIDs))
    {
        if (!mcNewSelectedGroupIDs.contains(group_id))
        {
            m_TaskGroupIDToImage_Attributes.remove(group_id);
            m_TaskGroupIDToImage_GanttChart.remove(group_id);
            m_GroupIDToRowImageHeight.remove(group_id);
            selection_changed = true;
        }
    }

    // Select items
    for (const int task_id : mcNewSelectedTaskIDs)
    {
        if (!m_SelectedTaskIDs.contains(task_id))
        {
            m_TaskItemIDToImage_Attributes.remove(task_id);
            m_TaskItemIDToImage_GanttChart.remove(task_id);
            m_TaskIDToRowImageHeight.remove(task_id);
            selection_changed = true;
        }
    }
    for (const int group_id : mcNewSelectedGroupIDs)
    {
        if (!m_SelectedGroupIDs.contains(group_id))
        {
            m_TaskGroupIDToImage_Attributes.remove(group_id);
            m_TaskGroupIDToImage_GanttChart.remove(group_id);
            m_GroupIDToRowImageHeight.remove(group_id);
            selection_changed = true;
        }
    }

    // Set new selection
    if (selection_changed)
    {
        m_SelectedTaskIDs = mcNewSelectedTaskIDs;
        m_SelectedGroupIDs = mcNewSelectedGroupIDs;

        // Update visuals
        update();

        // Let the world know
        emit SelectionChanged(m_SelectedTaskIDs, m_SelectedGroupIDs);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Select task IDs only
void ProjectEditor::SetSelectedTaskIDs(const QSet < int > mcNewSelectedTaskIDs)
{
    QList < QString > all_selected_task_ids;
    for (const int task_id : mcNewSelectedTaskIDs)
    {
        all_selected_task_ids << QString::number(task_id);
    }
    std::sort(all_selected_task_ids.begin(), all_selected_task_ids.end());
    CALL_IN(QString("mcNewSelectedTaskIDs={%1}")
        .arg(all_selected_task_ids.join(", ")));

    SetSelection(mcNewSelectedTaskIDs, QSet < int >());

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Select group IDs only
void ProjectEditor::SetSelectedGroupIDs(
    const QSet < int > mcNewSelectedGroupIDs)
{
    QList < QString > all_selected_group_ids;
    for (const int group_id : mcNewSelectedGroupIDs)
    {
        all_selected_group_ids << QString::number(group_id);
    }
    std::sort(all_selected_group_ids.begin(), all_selected_group_ids.end());
    CALL_IN(QString("mcNewSelectedGroupIDs={%1}")
        .arg(all_selected_group_ids.join(", ")));

    SetSelection(QSet < int >(), mcNewSelectedGroupIDs);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Selected task IDs only
QSet < int > ProjectEditor::GetSelectedTaskIDs() const
{
    CALL_IN("");

    CALL_OUT("");
    return m_SelectedTaskIDs;
}



///////////////////////////////////////////////////////////////////////////////
// Selected group IDs only
QSet < int > ProjectEditor::GetSelectedGroupIDs() const
{
    CALL_IN("");

    CALL_OUT("");
    return m_SelectedGroupIDs;
}



///////////////////////////////////////////////////////////////////////////////
// Determine which index is at a given position
int ProjectEditor::GetIndexAtPosition(const int mcX, const int mcY) const
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // Not using X right now (may do so in the future)
    Q_UNUSED(mcX)

    // Check for element being hovered
    for (int index = 0; index < m_VisibleIDs.size(); index++)
    {
        // Early stop: y coordinates are in ascending order. If we haven't
        // found a matching element yet, all the following elements will be
        // looking for a bigger y coordinate and thus not match.
        if (mcY < m_VisibleIDTopCoordinates[index])
        {
            break;
        }
        if (mcY >= m_VisibleIDTopCoordinates[index] &&
            mcY < m_VisibleIDBottomCoordinates[index])
        {
            CALL_OUT("");
            return index;
        }
    }

    // Not found
    CALL_OUT("");
    return INVALID_INDEX;
}



// ============================================================ GUI: Attributes



///////////////////////////////////////////////////////////////////////////////
// Initialize attribute pieces of information
void ProjectEditor::Initialize_Attributes()
{
    CALL_IN("");

    // Attribute titles - for serialization
    m_AttributeSerializationTitles.clear();
    m_AttributeSerializationTitles[Attribute_ID] = "id";
    m_AttributeSerializationTitles[Attribute_Title] = "title";
    m_AttributeSerializationTitles[Attribute_Duration] = "duration";
    m_AttributeSerializationTitles[Attribute_StartDate] = "start date";
    m_AttributeSerializationTitles[Attribute_FinishDate] = "finish date";
    m_AttributeSerializationTitles[Attribute_CriticalPath] = "critical path";
    m_AttributeSerializationTitles[Attribute_SlackWorkdays] = "slack (wd)";
    m_AttributeSerializationTitles[Attribute_SlackCalendarDays] = "slack (cd)";
    m_AttributeSerializationTitles[Attribute_Predecessors] = "predecessors";
    m_AttributeSerializationTitles[Attribute_Successors] = "successors";
    m_AttributeSerializationTitles[Attribute_CompletionStatus] =
        "completion status";
    m_AttributeSerializationTitles[Attribute_Resources] = "resources";
    m_AttributeSerializationTitles[Attribute_Attachments] = "attachments";
    m_AttributeSerializationTitles[Attribute_Comments] = "comments";
    m_AttributeSerializationTitles[Attribute_GanttChart] = "gantt chart";

    // Attribute display formats - titles for serialization
    m_AttributeDisplayFormatSerializationTitles.clear();
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_Default] = "default";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_TitleOnly] = "task name only";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_TitleAndParentGroup] = "task name and group";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_TitleAndFullHierarchy] =
            "task name and full group hiararchy";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_LongDateOnly] = "long date only";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_LongDateWithWeekday] = "long date with weekday";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_ISODateOnly] = "iso date only";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_ISODateWithWeekday] = "iso date with weekday";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_YesNo] = "yes/no";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_RedGreen] = "red/green";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_ReferencesWithTitle] = "references with titles";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_ReferencesOnly] = "references only";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_CompletionStatusText] =
            "completion status text";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_CompletionStatusPercent] =
            "completion status percent";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_CompletionStatusSymbol] =
            "completion status symbol";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_CommentTitles] = "comment titles";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_CommentResponsibilities] =
            "comment responsibilities";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_GanttAutomatic] = "automatic";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_GanttDays] = "days";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_GanttWeekdays] = "weekdays";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_GanttWeeks] = "weeks";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_GanttMonths] = "months";
    m_AttributeDisplayFormatSerializationTitles[
        AttributeDisplayFormat_GanttYears] = "years";

    // Available display formats for attributes
    m_AttributeAvailableDisplayFormats.clear();
    m_AttributeAvailableDisplayFormats[Attribute_ID]
        << AttributeDisplayFormat_Default;
    m_AttributeAvailableDisplayFormats[Attribute_Title]
        << AttributeDisplayFormat_TitleOnly
        << AttributeDisplayFormat_TitleAndParentGroup
        << AttributeDisplayFormat_TitleAndFullHierarchy;
    m_AttributeAvailableDisplayFormats[Attribute_Duration]
        << AttributeDisplayFormat_Default;
    m_AttributeAvailableDisplayFormats[Attribute_StartDate]
        << AttributeDisplayFormat_LongDateOnly
        << AttributeDisplayFormat_LongDateWithWeekday
        << AttributeDisplayFormat_ISODateOnly
        << AttributeDisplayFormat_ISODateWithWeekday;
    m_AttributeAvailableDisplayFormats[Attribute_FinishDate]
        << AttributeDisplayFormat_LongDateOnly
        << AttributeDisplayFormat_LongDateWithWeekday
        << AttributeDisplayFormat_ISODateOnly
        << AttributeDisplayFormat_ISODateWithWeekday;
    m_AttributeAvailableDisplayFormats[Attribute_CriticalPath]
        << AttributeDisplayFormat_RedGreen << AttributeDisplayFormat_YesNo;
    m_AttributeAvailableDisplayFormats[Attribute_SlackWorkdays]
        << AttributeDisplayFormat_Default;
    m_AttributeAvailableDisplayFormats[Attribute_SlackCalendarDays]
        << AttributeDisplayFormat_Default;
    m_AttributeAvailableDisplayFormats[Attribute_Predecessors]
        << AttributeDisplayFormat_ReferencesWithTitle
        << AttributeDisplayFormat_ReferencesOnly;
    m_AttributeAvailableDisplayFormats[Attribute_Successors]
        << AttributeDisplayFormat_ReferencesWithTitle
        << AttributeDisplayFormat_ReferencesOnly;
    m_AttributeAvailableDisplayFormats[Attribute_CompletionStatus]
        << AttributeDisplayFormat_CompletionStatusText
        << AttributeDisplayFormat_CompletionStatusPercent
        << AttributeDisplayFormat_CompletionStatusSymbol;
    m_AttributeAvailableDisplayFormats[Attribute_Resources]
        << AttributeDisplayFormat_Default;
    m_AttributeAvailableDisplayFormats[Attribute_Attachments]
        << AttributeDisplayFormat_Default;
    m_AttributeAvailableDisplayFormats[Attribute_Comments]
        << AttributeDisplayFormat_CommentTitles
        << AttributeDisplayFormat_CommentResponsibilities;
    m_AttributeAvailableDisplayFormats[Attribute_GanttChart]
        << AttributeDisplayFormat_GanttAutomatic
        << AttributeDisplayFormat_GanttDays
        << AttributeDisplayFormat_GanttWeekdays
        << AttributeDisplayFormat_GanttWeeks
        << AttributeDisplayFormat_GanttMonths
        << AttributeDisplayFormat_GanttYears;

    // Visible attributes
    m_VisibleAttributes.clear();
    m_VisibleAttributes << Attribute_ID
        << Attribute_CompletionStatus
        << Attribute_Title
        << Attribute_Duration
        << Attribute_StartDate
        << Attribute_FinishDate
        << Attribute_Predecessors
        << Attribute_Successors
        << Attribute_Resources
        << Attribute_Attachments
        << Attribute_Comments
        << Attribute_GanttChart;

    // Attribute display formats
    m_AttributeDisplayFormat.clear();
    m_AttributeDisplayFormat[Attribute_ID] = AttributeDisplayFormat_Default;
    m_AttributeDisplayFormat[Attribute_Title] =
        AttributeDisplayFormat_TitleOnly;
    m_AttributeDisplayFormat[Attribute_Duration] =
        AttributeDisplayFormat_Default;
    m_AttributeDisplayFormat[Attribute_StartDate] =
        AttributeDisplayFormat_ISODateOnly;
    m_AttributeDisplayFormat[Attribute_FinishDate] =
        AttributeDisplayFormat_ISODateOnly;
    m_AttributeDisplayFormat[Attribute_CriticalPath] =
        AttributeDisplayFormat_RedGreen;
    m_AttributeDisplayFormat[Attribute_SlackWorkdays] =
        AttributeDisplayFormat_Default;
    m_AttributeDisplayFormat[Attribute_SlackCalendarDays] =
        AttributeDisplayFormat_Default;
    m_AttributeDisplayFormat[Attribute_Predecessors] =
        AttributeDisplayFormat_ReferencesWithTitle;
    m_AttributeDisplayFormat[Attribute_Successors] =
        AttributeDisplayFormat_ReferencesWithTitle;
    m_AttributeDisplayFormat[Attribute_CompletionStatus] =
        AttributeDisplayFormat_CompletionStatusSymbol;
    m_AttributeDisplayFormat[Attribute_Resources] =
        AttributeDisplayFormat_Default;
    m_AttributeDisplayFormat[Attribute_Attachments] =
        AttributeDisplayFormat_Default;
    m_AttributeDisplayFormat[Attribute_Comments] =
        AttributeDisplayFormat_CommentTitles;
    m_AttributeDisplayFormat[Attribute_GanttChart] =
        AttributeDisplayFormat_GanttAutomatic;

    // Attribute alignment
    m_AttributeContentAlignment.clear();
    m_AttributeContentAlignment[Attribute_ID] = "center";
    m_AttributeContentAlignment[Attribute_Title] = "";
    m_AttributeContentAlignment[Attribute_Duration] = "center";
    m_AttributeContentAlignment[Attribute_StartDate] = "center";
    m_AttributeContentAlignment[Attribute_FinishDate] = "center";
    m_AttributeContentAlignment[Attribute_CriticalPath] = "center";
    m_AttributeContentAlignment[Attribute_SlackWorkdays] = "center";
    m_AttributeContentAlignment[Attribute_SlackCalendarDays] = "center";
    m_AttributeContentAlignment[Attribute_Predecessors] = "";
    m_AttributeContentAlignment[Attribute_Successors] = "";
    m_AttributeContentAlignment[Attribute_CompletionStatus] = "center";
    m_AttributeContentAlignment[Attribute_Resources] = "";
    m_AttributeContentAlignment[Attribute_Attachments] = "";
    m_AttributeContentAlignment[Attribute_Comments] = "";
    m_AttributeContentAlignment[Attribute_GanttChart] = "";

    // Attribute titles - for GUI
    m_AttributeGUITitles.clear();
    m_AttributeGUITitles[Attribute_ID] = tr("ID");
    m_AttributeGUITitles[Attribute_Title] = tr("Title");
    m_AttributeGUITitles[Attribute_Duration] = tr("Duration");
    m_AttributeGUITitles[Attribute_StartDate] = tr("Start");
    m_AttributeGUITitles[Attribute_FinishDate] = tr("Finish");
    m_AttributeGUITitles[Attribute_CriticalPath] = tr("Critical");
    m_AttributeGUITitles[Attribute_SlackWorkdays] = tr("Slack (wd)");
    m_AttributeGUITitles[Attribute_SlackCalendarDays] = tr("Slack (cd)");
    m_AttributeGUITitles[Attribute_Predecessors] = tr("Predecessors");
    m_AttributeGUITitles[Attribute_Successors] = tr("Successors");
    m_AttributeGUITitles[Attribute_CompletionStatus] = tr("Status");
    m_AttributeGUITitles[Attribute_Resources] = tr("Resources");
    m_AttributeGUITitles[Attribute_Attachments] = tr("Attachments");
    m_AttributeGUITitles[Attribute_Comments] = tr("Comments");
    m_AttributeGUITitles[Attribute_GanttChart] = tr("Gantt Chart");

    // Attribute display formats - titles for GUI
    m_AttributeDisplayFormatGUITitles.clear();
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_Default] = tr("");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_TitleOnly] =
        tr("Name Only");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_TitleAndParentGroup]
        = tr("Task name with parent group");
    m_AttributeDisplayFormatGUITitles[
        AttributeDisplayFormat_TitleAndFullHierarchy] =
        tr("Task name with full hierarchy");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_LongDateOnly] =
        tr("Long date");
    m_AttributeDisplayFormatGUITitles[
        AttributeDisplayFormat_LongDateWithWeekday] =
            tr("Long date with weekday");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_ISODateOnly] =
        tr("ISO date");
    m_AttributeDisplayFormatGUITitles[
        AttributeDisplayFormat_ISODateWithWeekday] =
            tr("ISO date with weekday");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_YesNo] =
        tr("Yes/no");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_RedGreen] =
        tr("Red/green flag");
    m_AttributeDisplayFormatGUITitles[
        AttributeDisplayFormat_ReferencesWithTitle] =
            tr("References with titles");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_ReferencesOnly] =
        tr("References only");
    m_AttributeDisplayFormatGUITitles[
        AttributeDisplayFormat_CompletionStatusText] = tr("Text");
    m_AttributeDisplayFormatGUITitles[
        AttributeDisplayFormat_CompletionStatusPercent] = tr("Percent");
    m_AttributeDisplayFormatGUITitles[
        AttributeDisplayFormat_CompletionStatusSymbol] = tr("Symbol");
    m_AttributeDisplayFormatGUITitles[
        AttributeDisplayFormat_CommentTitles] = tr("Comment");
    m_AttributeDisplayFormatGUITitles[
        AttributeDisplayFormat_CommentResponsibilities] =
            tr("Comment Responsibilities");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_GanttAutomatic] =
        tr("Automatic");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_GanttDays] =
        tr("Days");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_GanttWeekdays] =
        tr("Weekdays");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_GanttWeeks] =
        tr("Weeks");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_GanttMonths] =
        tr("Months");
    m_AttributeDisplayFormatGUITitles[AttributeDisplayFormat_GanttYears] =
        tr("Years");

    // Default width for each attribute
    m_AttributeWidths.clear();
    m_AttributeWidths[Attribute_ID] = 50;
    m_AttributeWidths[Attribute_Title] = 200;
    m_AttributeWidths[Attribute_Duration] = 100;
    m_AttributeWidths[Attribute_StartDate] = 100;
    m_AttributeWidths[Attribute_FinishDate] = 100;
    m_AttributeWidths[Attribute_CriticalPath] = 70;
    m_AttributeWidths[Attribute_SlackWorkdays] = 100;
    m_AttributeWidths[Attribute_SlackCalendarDays] = 100;
    m_AttributeWidths[Attribute_Predecessors] = 150;
    m_AttributeWidths[Attribute_Successors] = 150;
    m_AttributeWidths[Attribute_CompletionStatus] = 70;
    m_AttributeWidths[Attribute_Resources] = 150;
    m_AttributeWidths[Attribute_Attachments] = 150;
    m_AttributeWidths[Attribute_Comments] = 150;
    m_AttributeWidths[Attribute_GanttChart] = 1000;

    // Minimum width of an attribute
    m_MinAttributeWidth = 50;

    // Calculate total width of visible attributes
    CalculateAttributesTotalWidth();

    // Vertical padding before and after row content
    m_RowPadding = 2;
    // Horizontal padding before and after attribute content
    m_AttributePadding = 5;

    // Indentation scale for each level of task groups
    m_IndentScale = 20;
    // Groups expansion triangle: vertical padding before the triangle
    m_TrianglePadding = 6;
    // Groups expansion triangle: width and height
    m_TriangleWidth = 10;
    m_TriangleHeight = 10;
    // Groups expansion triangle: offset between triangle and subsequent text
    m_TrianglePostOffset = 3;

    // Separator margin (for resizing attributes)
    m_SeparatorDragMargin = 4;

    // Header image (attributes)
    m_HeaderImage_Attributes = QImage();

    // Row items images (attributes)
    m_TaskItemIDToImage_Attributes.clear();
    m_TaskGroupIDToImage_Attributes.clear();

    // Relationship to AllTaskItems::Information
    // (this lists required updates for this particular task only; if, e.g.
    // the reference changes, other tasks linking to and from the task that
    // changed will need to be updated as well - that's handled separately)
    const QList < Attributes > all_attributes = m_AttributeGUITitles.keys();
    m_TaskInformationToAffectedAttributes.clear();
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_ActualStart] << Attribute_StartDate;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_ActualFinish] << Attribute_FinishDate;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_Reference] << Attribute_ID;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_Title] << Attribute_Title;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_SchedulingMode] = QList < Attributes >();
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_FixedStartDate] = QList < Attributes >();
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_DurationValue] << Attribute_Duration;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_DurationUnits] << Attribute_Duration;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_CompletionStatus] << Attribute_GanttChart
            << Attribute_CompletionStatus;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_TextColor] = all_attributes;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_BackgroundColor] = all_attributes;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_TextStyle] = all_attributes;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_EarlyStart] << Attribute_StartDate;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_EarlyFinish] << Attribute_FinishDate;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_LateStart] = QList < Attributes >();
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_LateFinish] = QList < Attributes >();
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_SlackWorkdays] << Attribute_SlackWorkdays;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_SlackCalendarDays]
            << Attribute_SlackCalendarDays;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_IsMilestone] << Attribute_GanttChart;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_IsOnCriticalPath]  << Attribute_CriticalPath
            << Attribute_GanttChart;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_Attachments] << Attribute_Attachments;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_Comments] << Attribute_Comments;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_Resources] << Attribute_Resources;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_LinkedTasks] << Attribute_Predecessors
            << Attribute_Successors;
    m_TaskInformationToAffectedAttributes[
        AllTaskItems::Information_AnyInformation] = all_attributes;

    // Relationship to AllTaskGroups::Information
    m_GroupInformationToAffectedAttributes.clear();
    m_GroupInformationToAffectedAttributes[
        AllTaskGroups::Information_BackgroundColor] = all_attributes;
    m_GroupInformationToAffectedAttributes[
        AllTaskGroups::Information_CompletionValue]
            << Attribute_CompletionStatus;
    m_GroupInformationToAffectedAttributes[
        AllTaskGroups::Information_ParentGroupID] = QList < Attributes >();
    m_GroupInformationToAffectedAttributes[
        AllTaskGroups::Information_TextColor] = all_attributes;
    m_GroupInformationToAffectedAttributes[
        AllTaskGroups::Information_TextStyle] = all_attributes;
    m_GroupInformationToAffectedAttributes[
        AllTaskGroups::Information_Title] << Attribute_Title;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes
QPair < QStringList, QList < int > > ProjectEditor::GetTaskContent(
    const int mcTaskID,
    const ProjectEditor::Attributes mcAttribute)
{
    CALL_IN(QString("mcTaskID=%1, mcAttribute=\"%2\"")
        .arg(QString::number(mcTaskID),
             m_AttributeSerializationTitles[mcAttribute]));

    // Internal - no checks

    // Make sure content is available in cache
    if (!m_TaskIDAttributeContent.contains(mcTaskID) ||
        !m_TaskIDAttributeContent[mcTaskID].contains(mcAttribute))
    {
        AllTaskItems * at = AllTaskItems::Instance();
        QHash < AllTaskItems::Information, QString > task_info =
            at -> GetInformation(mcTaskID);
        switch (mcAttribute)
        {
        case Attribute_Attachments:
            UpdateTaskContent_Attachments(mcTaskID, task_info);
            break;

        case Attribute_Comments:
            UpdateTaskContent_Comments(mcTaskID, task_info);
            break;

        case Attribute_CompletionStatus:
            UpdateTaskContent_CompletionStatus(mcTaskID, task_info);
            break;

        case Attribute_CriticalPath:
            UpdateTaskContent_CriticalPath(mcTaskID, task_info);
            break;

        case Attribute_Duration:
            UpdateTaskContent_Duration(mcTaskID, task_info);
            break;

        case Attribute_FinishDate:
            UpdateTaskContent_FinishDate(mcTaskID, task_info);
            break;

        case Attribute_ID:
            UpdateTaskContent_ID(mcTaskID, task_info);
            break;

        case Attribute_Predecessors:
            UpdateTaskContent_Predecessors(mcTaskID, task_info);
            break;

        case Attribute_Resources:
            UpdateTaskContent_Resources(mcTaskID, task_info);
            break;

        case Attribute_SlackCalendarDays:
            UpdateTaskContent_SlackCalendarDays(mcTaskID, task_info);
            break;

        case Attribute_SlackWorkdays:
            UpdateTaskContent_SlackWorkdays(mcTaskID, task_info);
            break;

        case Attribute_StartDate:
            UpdateTaskContent_StartDate(mcTaskID, task_info);
            break;

        case Attribute_Successors:
            UpdateTaskContent_Successors(mcTaskID, task_info);
            break;

        case Attribute_Title:
            UpdateTaskContent_Title(mcTaskID, task_info);
            break;

        default:
        {
            // Error
            const QString reason = tr("Unknown attribute \"%1\"")
                .arg(m_AttributeSerializationTitles[mcAttribute]);
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return QPair < QStringList, QList < int > >();
        }
        }

        // Apply alignment and style
        QString html_pre;
        QString html_post;
        if (!m_AttributeContentAlignment[mcAttribute].isEmpty())
        {
            html_pre = QString("<p align=\"%1\">")
                .arg(m_AttributeContentAlignment[Attribute_ID]);
            html_post = "</p>";
        }
        // Style
        if (task_info[AllTaskItems::Information_TextStyle] == "bold")
        {
            html_pre += QString("<b>");
            html_post = QString("</b>") + html_post;
        } else if (
            task_info[AllTaskItems::Information_TextStyle] == "italics")
        {
            html_pre += QString("<i>");
            html_post = QString("</i>") + html_post;
        } else if (
            task_info[AllTaskItems::Information_TextStyle] == "bold italics")
        {
            html_pre += QString("<b><i>");
            html_post = QString("</i></b>") + html_post;
        } else
        {
            // Style "normal" does not require any markup.
        }

        // Apply style
        for (int item = 0;
             item < m_TaskIDAttributeContent[mcTaskID][mcAttribute].size();
             item++)
        {
            m_TaskIDAttributeContent[mcTaskID][mcAttribute][item] =
                QString("%1%2%3")
                    .arg(html_pre,
                         m_TaskIDAttributeContent[mcTaskID][mcAttribute][item],
                         html_post);
        }
    }

    // Return value
    CALL_OUT("");
    return QPair < QStringList, QList < int > >
        (m_TaskIDAttributeContent[mcTaskID][mcAttribute],
         m_TaskIDAttributeData[mcTaskID][mcAttribute]);
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: attachments
void ProjectEditor::UpdateTaskContent_Attachments(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    Q_UNUSED(mcrTaskInfo)

    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Abbrevation
    AllAttachments * aa = AllAttachments::Instance();
    AllTaskItems * at = AllTaskItems::Instance();

    // Attachments
    const QList < int > attachment_ids = at -> GetAttachmentIDs(mcTaskID);
    QHash < int, QString > all_attachments;
    for (int attachment_id : attachment_ids)
    {
        const QHash < AllAttachments::Information, QString > attachment_info =
            aa -> GetInformation(attachment_id);
        all_attachments[attachment_id] =
            attachment_info[AllAttachments::Information_Name];
    }
    const QList < int > sorted_attachment_ids =
        StringHelper::SortHash(all_attachments);

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_Attachments].clear();
    m_TaskIDAttributeData[mcTaskID][Attribute_Attachments].clear();
    for (const int attachment_id : sorted_attachment_ids)
    {
        m_TaskIDAttributeContent[mcTaskID][Attribute_Attachments]
            << all_attachments[attachment_id];
    }
    m_TaskIDAttributeData[mcTaskID][Attribute_Attachments] =
        sorted_attachment_ids;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: comments
void ProjectEditor::UpdateTaskContent_Comments(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    Q_UNUSED(mcrTaskInfo)

    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Abbrevation
    AllComments * ac = AllComments::Instance();
    AllTaskItems * at = AllTaskItems::Instance();

    // Content
    const QList < int > comment_ids = at -> GetCommentIDs(mcTaskID);
    switch (m_AttributeDisplayFormat[Attribute_Comments])
    {
    case AttributeDisplayFormat_CommentTitles:
    {
        // === All comment titles
        QHash < int, QString > all_comments;
        for (int comment_id : comment_ids)
        {
            const QHash < AllComments::Information, QString > comment_info =
                ac -> GetInformation(comment_id);
            all_comments[comment_id] =
                comment_info[AllComments::Information_Title];
        }
        const QList < int > sorted_comment_ids =
            StringHelper::SortHash(all_comments);

        // Update content cache
        m_TaskIDAttributeContent[mcTaskID][Attribute_Comments].clear();
        m_TaskIDAttributeData[mcTaskID][Attribute_Comments].clear();
        for (const int comment_id : sorted_comment_ids)
        {
            m_TaskIDAttributeContent[mcTaskID][Attribute_Comments]
                << all_comments[comment_id];
        }
        m_TaskIDAttributeData[mcTaskID][Attribute_Comments] =
            sorted_comment_ids;
        break;
    }

    case AttributeDisplayFormat_CommentResponsibilities:
    {
        // === All resources in all comments
        QStringList all_resources;
        for (int comment_id : comment_ids)
        {
            const QStringList comment_resources =
                ac -> GetResourcesMentioned(comment_id);
            for (const QString & this_resource : comment_resources)
            {
                if (!all_resources.contains(this_resource))
                {
                    all_resources << this_resource;
                }
            }
        }
        std::sort(all_resources.begin(), all_resources.end());

        // Update content cache
        m_TaskIDAttributeContent[mcTaskID][Attribute_Comments] = all_resources;
        m_TaskIDAttributeData[mcTaskID][Attribute_Comments] =
            QList < int >();
        break;
    }

    default:
    {
        // === Error
        const QString reason = tr("Unknown display format.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: completion status
void ProjectEditor::UpdateTaskContent_CompletionStatus(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Content
    QString content;
    switch (m_AttributeDisplayFormat[Attribute_CompletionStatus])
    {
    case AttributeDisplayFormat_CompletionStatusText:
        content = mcrTaskInfo[AllTaskItems::Information_CompletionStatus];
        break;

    case AttributeDisplayFormat_CompletionStatusPercent:
    {
        const QString status =
            mcrTaskInfo[AllTaskItems::Information_CompletionStatus];
        if (status == "not started")
        {
            content = QString("0%");
        } else if (status == "started")
        {
            content = QString("50%");
        } else if (status == "completed")
        {
            content = QString("100%");
        }
        break;
    }

    case AttributeDisplayFormat_CompletionStatusSymbol:
    {
        const QString status =
            mcrTaskInfo[AllTaskItems::Information_CompletionStatus];
        if (status == "not started")
        {
            content = QString("&hellip;");
        } else if (status == "started")
        {
            content = QString("<font color=\"blue\"><b>&#x21E8;</b></font>");
        } else if (status == "completed")
        {
            content = QString("<font color=\"green\"><b>&#x2713;</b></font>");
        }
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Unknown display format.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_CompletionStatus].clear();
    m_TaskIDAttributeContent[mcTaskID][Attribute_CompletionStatus] << content;
    m_TaskIDAttributeData[mcTaskID][Attribute_CompletionStatus] =
        QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: critical path
void ProjectEditor::UpdateTaskContent_CriticalPath(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Content
    QString content;
    switch (m_AttributeDisplayFormat[Attribute_CriticalPath])
    {
    case AttributeDisplayFormat_YesNo:
        content = mcrTaskInfo[AllTaskItems::Information_IsOnCriticalPath];
        break;

    case AttributeDisplayFormat_RedGreen:
        if (mcrTaskInfo[AllTaskItems::Information_IsOnCriticalPath] == "yes")
        {
            content = QString("<font color=\"red\">&#x25FC;</font>");
        } else
        {
            content = QString("<font color=\"green\">&#x25FC;</font>");
        }
        break;

    default:
    {
        // Error
        const QString reason = tr("Unknown display format.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_CriticalPath].clear();
    m_TaskIDAttributeContent[mcTaskID][Attribute_CriticalPath] << content;
    m_TaskIDAttributeData[mcTaskID][Attribute_CriticalPath] = QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: duration
void ProjectEditor::UpdateTaskContent_Duration(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Content
    const QString content = QString("%1%2")
        .arg(mcrTaskInfo[AllTaskItems::Information_DurationValue],
             mcrTaskInfo[AllTaskItems::Information_DurationUnits]);

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_Duration].clear();
    m_TaskIDAttributeContent[mcTaskID][Attribute_Duration] << content;
    m_TaskIDAttributeData[mcTaskID][Attribute_Duration] = QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: finish date
void ProjectEditor::UpdateTaskContent_FinishDate(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Get date
    QDate date;
    if (!mcrTaskInfo[AllTaskItems::Information_ActualFinish].isEmpty())
    {
        // There is an actual finish date
        date = QDate::fromString(
            mcrTaskInfo[AllTaskItems::Information_ActualFinish], "yyyy-MM-dd");
    } else
    {
        // Use planned finish date
        date = QDate::fromString(
            mcrTaskInfo[AllTaskItems::Information_EarlyFinish], "yyyy-MM-dd");
    }

    // ... and in the right format
    QString content;
    switch (m_AttributeDisplayFormat[Attribute_FinishDate])
    {
    case AttributeDisplayFormat_ISODateOnly:
        content = mcrTaskInfo[AllTaskItems::Information_EarlyFinish];
        break;

    case AttributeDisplayFormat_ISODateWithWeekday:
        content = date.toString("yyyy-MM-dd (ddd)");
        break;

    case AttributeDisplayFormat_LongDateOnly:
        content = date.toString("dd MMM yyyy");
        break;

    case AttributeDisplayFormat_LongDateWithWeekday:
        content = date.toString("ddd, dd MMM yyyy");
        break;

    default:
    {
        // Error
        const QString reason = tr("Unknown display format.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_FinishDate].clear();
    m_TaskIDAttributeContent[mcTaskID][Attribute_FinishDate] << content;
    m_TaskIDAttributeData[mcTaskID][Attribute_FinishDate] = QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: ID
void ProjectEditor::UpdateTaskContent_ID(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Content
    const QString content = mcrTaskInfo[AllTaskItems::Information_Reference];

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_ID].clear();
    m_TaskIDAttributeContent[mcTaskID][Attribute_ID] << content;
    m_TaskIDAttributeData[mcTaskID][Attribute_ID] = QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: predecessors
void ProjectEditor::UpdateTaskContent_Predecessors(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    Q_UNUSED(mcrTaskInfo)

    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Abbreviations
    AllTaskItems * at = AllTaskItems::Instance();
    AllTaskLinks * al = AllTaskLinks::Instance();

    // Predecessors
    const QList < int > link_ids = al -> GetIDsForSuccessorTaskID(mcTaskID);
    QHash < int, QString > all_links;
    for (const int link_id : qAsConst(link_ids))
    {
        // Get link information
        const QHash < AllTaskLinks::Information, QString > link_info =
            al -> GetInformation(link_id);

        // Get predecessor task information
        const int predecessor_id =
            link_info[AllTaskLinks::Information_PredecessorID].toInt();
        const QHash < AllTaskItems::Information, QString > predecessor_info =
            at -> GetInformation(predecessor_id);

        // Build text
        // ("23 (Get it done), ff-3 wd"
        QString predecessor_text;
        switch (m_AttributeDisplayFormat[Attribute_Predecessors])
        {
        case AttributeDisplayFormat_ReferencesWithTitle:
            predecessor_text = QString("%1 (%2)")
                .arg(predecessor_info[AllTaskItems::Information_Reference],
                    predecessor_info[AllTaskItems::Information_Title]);
            break;

        case AttributeDisplayFormat_ReferencesOnly:
            predecessor_text = QString("%1")
                .arg(predecessor_info[AllTaskItems::Information_Reference]);
            break;

        default:
        {
            // Error
            const QString reason = tr("Unknown display format.");
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return;
        }
        }
        if (link_info[AllTaskLinks::Information_ConnectionType] ==
            "finish to start")
        {
            if (link_info[AllTaskLinks::Information_LeadDuration].toInt() != 0)
            {
                predecessor_text += QString(", FS%1 %2")
                    .arg(link_info[AllTaskLinks::Information_LeadDuration],
                        link_info[AllTaskLinks::Information_LeadUnits]);
            }
            if (link_info[AllTaskLinks::Information_LagDuration].toInt() != 0)
            {
                predecessor_text += QString(", FS");
            }
        } else if (link_info[AllTaskLinks::Information_ConnectionType] ==
            "finish to finish")
        {
            predecessor_text += ", FF";
        } else if (link_info[AllTaskLinks::Information_ConnectionType] ==
            "start to finish")
        {
            predecessor_text += ", SF";
        } else if (link_info[AllTaskLinks::Information_ConnectionType] ==
            "start to start")
        {
            predecessor_text += ", SS";
        }
        if (link_info[AllTaskLinks::Information_LagDuration].toInt() != 0)
        {
            predecessor_text += QString("+%1 %2")
                .arg(link_info[AllTaskLinks::Information_LeadDuration],
                    link_info[AllTaskLinks::Information_LeadUnits]);
        }
        all_links[link_id] = predecessor_text;
    }
    const QList < int > sorted_link_ids = StringHelper::SortHash(all_links);

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_Predecessors].clear();
    m_TaskIDAttributeData[mcTaskID][Attribute_Predecessors].clear();
    for (const int link_id : sorted_link_ids)
    {
        m_TaskIDAttributeContent[mcTaskID][Attribute_Predecessors]
            << all_links[link_id];
    }
    m_TaskIDAttributeData[mcTaskID][Attribute_Predecessors] =
        sorted_link_ids;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: resources
void ProjectEditor::UpdateTaskContent_Resources(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    Q_UNUSED(mcrTaskInfo)

    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Abbreviations
    AllTaskItems * at = AllTaskItems::Instance();
    AllResources * ar = AllResources::Instance();

    // Resources
    const QList < int > resource_ids = at -> GetResourceIDs(mcTaskID);
    QHash < int, QString > all_resources;
    for (int resource_id : resource_ids)
    {
        const QHash < AllResources::Information, QString > resource_info =
            ar -> GetInformation(resource_id);
        all_resources[resource_id] =
            resource_info[AllResources::Information_Name];
    }
    const QList < int > sorted_resource_ids =
        StringHelper::SortHash(all_resources);

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_Resources].clear();
    m_TaskIDAttributeData[mcTaskID][Attribute_Resources].clear();
    for (const int resource_id : sorted_resource_ids)
    {
        m_TaskIDAttributeContent[mcTaskID][Attribute_Resources]
            << all_resources[resource_id];
    }
    m_TaskIDAttributeData[mcTaskID][Attribute_Resources] =
        sorted_resource_ids;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: slack (calendar days)
void ProjectEditor::UpdateTaskContent_SlackCalendarDays(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Content
    const QString content =
        mcrTaskInfo[AllTaskItems::Information_SlackCalendarDays];

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_SlackCalendarDays].clear();
    m_TaskIDAttributeContent[mcTaskID][Attribute_SlackCalendarDays] << content;
    m_TaskIDAttributeData[mcTaskID][Attribute_SlackCalendarDays] =
        QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: slack (workdays)
void ProjectEditor::UpdateTaskContent_SlackWorkdays(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Content
    const QString content =
        mcrTaskInfo[AllTaskItems::Information_SlackCalendarDays];

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_SlackCalendarDays].clear();
    m_TaskIDAttributeContent[mcTaskID][Attribute_SlackCalendarDays] << content;
    m_TaskIDAttributeData[mcTaskID][Attribute_SlackCalendarDays] =
        QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: start date
void ProjectEditor::UpdateTaskContent_StartDate(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Get date
    QDate date;
    if (!mcrTaskInfo[AllTaskItems::Information_ActualStart].isEmpty())
    {
        // There is an actual start date
        date = QDate::fromString(
            mcrTaskInfo[AllTaskItems::Information_ActualStart], "yyyy-MM-dd");
    } else
    {
        // Use planned start date
        date = QDate::fromString(
            mcrTaskInfo[AllTaskItems::Information_EarlyStart], "yyyy-MM-dd");
    }

    // ... and in the right format
    QString content;
    switch (m_AttributeDisplayFormat[Attribute_StartDate])
    {
    case AttributeDisplayFormat_ISODateOnly:
        content = date.toString("yyyy-MM-dd");
        break;

    case AttributeDisplayFormat_ISODateWithWeekday:
        content = date.toString("yyyy-MM-dd (ddd)");
        break;

    case AttributeDisplayFormat_LongDateOnly:
        content = date.toString("dd MMM yyyy");
        break;

    case AttributeDisplayFormat_LongDateWithWeekday:
        content = date.toString("ddd, dd MMM yyyy");
        break;

    default:
    {
        // Error
        const QString reason = tr("Unknown display format.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_StartDate].clear();
    m_TaskIDAttributeContent[mcTaskID][Attribute_StartDate] << content;
    m_TaskIDAttributeData[mcTaskID][Attribute_StartDate] = QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: successors
void ProjectEditor::UpdateTaskContent_Successors(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    Q_UNUSED(mcrTaskInfo)

    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Abbreviations
    AllTaskItems * at = AllTaskItems::Instance();
    AllTaskLinks * al = AllTaskLinks::Instance();

    // Predecessors
    const QList < int > link_ids = al -> GetIDsForPredecessorTaskID(mcTaskID);
    QHash < int, QString > all_links;
    for (const int link_id : qAsConst(link_ids))
    {
        // Get link information
        const QHash < AllTaskLinks::Information, QString > link_info =
            al -> GetInformation(link_id);

        // Get successor task information
        const int successor_id =
            link_info[AllTaskLinks::Information_SuccessorID].toInt();
        const QHash < AllTaskItems::Information, QString > successor_info =
            at -> GetInformation(successor_id);

        // Build text
        // ("23 (Get it done), ff-3 wd"
        QString successor_text;
        switch (m_AttributeDisplayFormat[Attribute_Successors])
        {
        case AttributeDisplayFormat_ReferencesWithTitle:
            successor_text = QString("%1 (%2)")
                .arg(successor_info[AllTaskItems::Information_Reference],
                    successor_info[AllTaskItems::Information_Title]);
            break;

        case AttributeDisplayFormat_ReferencesOnly:
            successor_text = QString("%1")
                .arg(successor_info[AllTaskItems::Information_Reference]);
            break;

        default:
        {
            // Error
            const QString reason = tr("Unknown display format.");
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return;
        }
        }
        if (link_info[AllTaskLinks::Information_ConnectionType] ==
            "finish to start")
        {
            if (link_info[AllTaskLinks::Information_LeadDuration].toInt() != 0)
            {
                successor_text += QString(", FS%1 %2")
                    .arg(link_info[AllTaskLinks::Information_LeadDuration],
                        link_info[AllTaskLinks::Information_LeadUnits]);
            }
            if (link_info[AllTaskLinks::Information_LagDuration].toInt() != 0)
            {
                successor_text += QString(", FS");
            }
        } else if (link_info[AllTaskLinks::Information_ConnectionType] ==
            "finish to finish")
        {
            successor_text += ", FF";
        } else if (link_info[AllTaskLinks::Information_ConnectionType] ==
            "start to finish")
        {
            successor_text += ", SF";
        } else if (link_info[AllTaskLinks::Information_ConnectionType] ==
            "start to start")
        {
            successor_text += ", SS";
        }
        if (link_info[AllTaskLinks::Information_LagDuration].toInt() != 0)
        {
            successor_text += QString("+%1 %2")
                .arg(link_info[AllTaskLinks::Information_LeadDuration],
                    link_info[AllTaskLinks::Information_LeadUnits]);
        }
        all_links[link_id] = successor_text;
    }
    const QList < int > sorted_link_ids = StringHelper::SortHash(all_links);

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_Successors].clear();
    m_TaskIDAttributeData[mcTaskID][Attribute_Successors].clear();
    for (const int link_id : sorted_link_ids)
    {
        m_TaskIDAttributeContent[mcTaskID][Attribute_Successors]
            << all_links[link_id];
    }
    m_TaskIDAttributeData[mcTaskID][Attribute_Successors] =
        sorted_link_ids;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task attributes: title
void ProjectEditor::UpdateTaskContent_Title(
    const int mcTaskID,
    const QHash < AllTaskItems::Information, QString > & mcrTaskInfo)
{
    CALL_IN(QString("mcTaskID=%1, mcrTaskInfo=...")
        .arg(QString::number(mcTaskID)));

    // Content
    AllTaskGroups * ag = AllTaskGroups::Instance();
    QString content;
    AttributeDisplayFormat format = m_AttributeDisplayFormat[Attribute_Title];
    switch (format)
    {
    case AttributeDisplayFormat_TitleOnly:
        content = mcrTaskInfo[AllTaskItems::Information_Title];
        break;

    case AttributeDisplayFormat_TitleAndParentGroup:
    {
        const int parent_id = ag -> GetParentGroupIDForTaskID(mcTaskID);
        if (parent_id != AllTaskGroups::ROOT_ID)
        {
            const QHash < AllTaskGroups::Information, QString > parent_info =
                ag -> GetInformation(parent_id);
            content = QString("%1: %2")
                .arg(parent_info[AllTaskGroups::Information_Title],
                     mcrTaskInfo[AllTaskItems::Information_Title]);
        } else
        {
            content = mcrTaskInfo[AllTaskItems::Information_Title];
        }
        break;
    }

    case AttributeDisplayFormat_TitleAndFullHierarchy:
    {
        QStringList parent_names;
        int parent_id = ag -> GetParentGroupIDForTaskID(mcTaskID);
        while (parent_id != AllTaskGroups::ROOT_ID)
        {
            const QHash < AllTaskGroups::Information, QString > parent_info =
                ag -> GetInformation(parent_id);
            parent_names << parent_info[AllTaskGroups::Information_Title];
            parent_id = ag -> GetParentGroupIDForGroupID(parent_id);
        }
        content = QString("%1: %2")
            .arg(parent_names.join("/"),
                 mcrTaskInfo[AllTaskItems::Information_Title]);
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Unknown display format \"%1\".")
            .arg(m_AttributeDisplayFormatSerializationTitles[format]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    // Update content cache
    m_TaskIDAttributeContent[mcTaskID][Attribute_Title].clear();
    m_TaskIDAttributeContent[mcTaskID][Attribute_Title] << content;
    m_TaskIDAttributeData[mcTaskID][Attribute_Title] = QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task group attributes
QPair < QStringList, QList < int > > ProjectEditor::GetGroupContent(
    const int mcGroupID,
    const ProjectEditor::Attributes mcAttribute)
{
    CALL_IN(QString("mcGroupID=%1, mcAttribute=\"%2\"")
        .arg(QString::number(mcGroupID),
             m_AttributeSerializationTitles[mcAttribute]));

    // Internal - no checks

    // Make sure content is available in cache
    if (!m_GroupIDAttributeContent.contains(mcGroupID) ||
        !m_GroupIDAttributeContent[mcGroupID].contains(mcAttribute))
    {
        AllTaskGroups * ag = AllTaskGroups::Instance();
        QHash < AllTaskGroups::Information, QString > group_info =
            ag -> GetInformation(mcGroupID);
        switch (mcAttribute)
        {
        case Attribute_Attachments:
            // Falls through
        case Attribute_Comments:
            // Nothing to do
            CALL_OUT("");
            return QPair < QStringList, QList < int > >();

        case Attribute_CompletionStatus:
            UpdateGroupContent_CompletionStatus(mcGroupID, group_info);
            break;

        case Attribute_CriticalPath:
            // Falls through
        case Attribute_Duration:
            // Falls through
        case Attribute_FinishDate:
            // Falls through
        case Attribute_ID:
            // Falls through
        case Attribute_Predecessors:
            // Falls through
        case Attribute_Resources:
            // Falls through
        case Attribute_SlackCalendarDays:
            // Falls through
        case Attribute_SlackWorkdays:
            // Falls through
        case Attribute_StartDate:
            // Falls through
        case Attribute_Successors:
            // Nothing to do
            CALL_OUT("");
            return QPair < QStringList, QList < int > >();

        case Attribute_Title:
            UpdateGroupContent_Title(mcGroupID, group_info);
            break;

        default:
        {
            // Error
            const QString reason = tr("Unknown attribute \"%1\"")
                .arg(m_AttributeSerializationTitles[mcAttribute]);
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return QPair < QStringList, QList < int > >();
        }
        }

        // Apply alignment and style
        QString html_pre;
        QString html_post;
        if (!m_AttributeContentAlignment[mcAttribute].isEmpty())
        {
            html_pre = QString("<p align=\"%1\">")
                .arg(m_AttributeContentAlignment[Attribute_ID]);
            html_post = "</p>";
        }
        // Style
        if (group_info[AllTaskGroups::Information_TextStyle] == "bold")
        {
            html_pre += QString("<b>");
            html_post = QString("</b>") + html_post;
        } else if (
            group_info[AllTaskGroups::Information_TextStyle] == "italics")
        {
            html_pre += QString("<i>");
            html_post = QString("</i>") + html_post;
        } else if (
            group_info[AllTaskGroups::Information_TextStyle] == "bold italics")
        {
            html_pre += QString("<b><i>");
            html_post = QString("</i></b>") + html_post;
        } else
        {
            // Style "normal" does not require any markup.
        }

        // Apply style
        for (int item = 0;
             item < m_GroupIDAttributeContent[mcGroupID][mcAttribute].size();
             item++)
        {
            m_GroupIDAttributeContent[mcGroupID][mcAttribute][item] =
                html_pre
                + m_GroupIDAttributeContent[mcGroupID][mcAttribute][item]
                + html_post;
        }
    }

    // Return value
    CALL_OUT("");
    return QPair < QStringList, QList < int > >
        (m_GroupIDAttributeContent[mcGroupID][mcAttribute],
         m_GroupIDAttributeData[mcGroupID][mcAttribute]);
}



///////////////////////////////////////////////////////////////////////////////
// Content for task group attributes: completion status
void ProjectEditor::UpdateGroupContent_CompletionStatus(
    const int mcGroupID,
    const QHash < AllTaskGroups::Information, QString > & mcrGroupInfo)
{
    CALL_IN(QString("mcGroupID=%1, mcrGroupInfo=...")
        .arg(QString::number(mcGroupID)));

    // Content
    QString content;
    switch (m_AttributeDisplayFormat[Attribute_CompletionStatus])
    {
    case AttributeDisplayFormat_CompletionStatusText:
        // Do nothing
        break;

    case AttributeDisplayFormat_CompletionStatusPercent:
    {
        content = mcrGroupInfo[AllTaskGroups::Information_CompletionValue];
        break;
    }

    case AttributeDisplayFormat_CompletionStatusSymbol:
        // Do nothing.
        break;

    default:
    {
        // Error
        const QString reason = tr("Unknown display format.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    // Update content cache
    m_GroupIDAttributeContent[mcGroupID][Attribute_CompletionStatus].clear();
    m_GroupIDAttributeContent[mcGroupID][Attribute_CompletionStatus] <<
        content;
    m_GroupIDAttributeData[mcGroupID][Attribute_CompletionStatus] =
        QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Content for task group attributes: title
void ProjectEditor::UpdateGroupContent_Title(const int mcGroupID,
    const QHash < AllTaskGroups::Information, QString > & mcrGroupInfo)
{
    CALL_IN(QString("mcGroupID=%1, mcrGroupInfo=...")
        .arg(QString::number(mcGroupID)));

    // Abbreviation
    AllTaskGroups * ag = AllTaskGroups::Instance();
    int parent_id =
        mcrGroupInfo[AllTaskGroups::Information_ParentGroupID].toInt();

    // Content
    QString content;
    switch (m_AttributeDisplayFormat[Attribute_Title])
    {
    case AttributeDisplayFormat_TitleOnly:
        content = mcrGroupInfo[AllTaskGroups::Information_Title];
        break;

    case AttributeDisplayFormat_TitleAndParentGroup:
    {
        if (parent_id != AllTaskGroups::ROOT_ID)
        {
            const QHash < AllTaskGroups::Information, QString > parent_info =
                ag -> GetInformation(parent_id);
            content = QString("%1: %2")
                .arg(parent_info[AllTaskGroups::Information_Title],
                     mcrGroupInfo[AllTaskGroups::Information_Title]);
        } else
        {
            content = mcrGroupInfo[AllTaskGroups::Information_Title];
        }
        break;
    }

    case AttributeDisplayFormat_TitleAndFullHierarchy:
    {
        QStringList parent_names;
        while (parent_id != AllTaskGroups::ROOT_ID)
        {
            const QHash < AllTaskGroups::Information, QString > parent_info =
                ag -> GetInformation(parent_id);
            parent_names << parent_info[AllTaskGroups::Information_Title];
            parent_id = ag -> GetParentGroupIDForGroupID(parent_id);
        }
        content = QString("%1: %2")
            .arg(parent_names.join("/"),
                 mcrGroupInfo[AllTaskGroups::Information_Title]);
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Unknown display format.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    // Update content cache
    m_GroupIDAttributeContent[mcGroupID][Attribute_Title].clear();
    m_GroupIDAttributeContent[mcGroupID][Attribute_Title] << content;
    m_GroupIDAttributeData[mcGroupID][Attribute_Title] = QList < int >();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Recalculate total with of all visible attributes
void ProjectEditor::CalculateAttributesTotalWidth()
{
    CALL_IN("");

    m_AttributesTotalWidth = 0;
    m_VisibleAttributesLeftCoordinates.clear();
    m_VisibleAttributesRightCoordinates.clear();
    for (const Attributes attribute : qAsConst(m_VisibleAttributes))
    {
        if (attribute == Attribute_GanttChart)
        {
            // Is completely separate
            continue;
        }

        // Save start (left) coordinate
        m_VisibleAttributesLeftCoordinates << m_AttributesTotalWidth;

        // Sum up total attribute width
        m_AttributesTotalWidth += m_AttributeWidths[attribute];

        // Save end (right) coordinate
        m_VisibleAttributesRightCoordinates << m_AttributesTotalWidth;
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Minimum Header height (attributes)
int ProjectEditor::CalculateMinimumHeaderHeight_Attributes()
{
    CALL_IN("");

    // Alignment
    QString html_pre = "<b><p align=\"center\">";
    QString html_post = "</p></b>";

    // Calculate header height
    int header_height = 0;
    for (const Attributes attribute : qAsConst(m_VisibleAttributes))
    {
        if (!m_AttributeGUITitles.contains(attribute))
        {
            // Error
            const QString reason =
                tr("No title found for this attribute type.");
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT("");
            return 0;
        }

        // Save content
        const QString html = QString("%1%2%3")
            .arg(html_pre,
                m_AttributeGUITitles[attribute],
                html_post);

        // Determine required height
        QTextDocument text;
        text.setHtml(html);
        text.setDocumentMargin(0.0);
        text.setDefaultFont(m_DefaultFont);
        const int effective_width =
            m_AttributeWidths[attribute] - 2*m_AttributePadding;
        text.setTextWidth(effective_width);
        const int content_height = text.size().height();

        // Determine total required height
        header_height = qMax(header_height, content_height);
    }

    CALL_OUT("");
    return header_height;
}



///////////////////////////////////////////////////////////////////////////////
// Header image: attributes
void ProjectEditor::UpdateHeaderImage_Attributes()
{
    CALL_IN("");

    // Private method - no checks

    // Alignment
    QString html_pre = "<b><p align=\"center\">";
    QString html_post = "</p></b>";

    // Determine row content
    QHash < Attributes, QString > html_content;
    for (const Attributes attribute : qAsConst(m_VisibleAttributes))
    {
        if (!m_AttributeGUITitles.contains(attribute))
        {
            // Error
            const QString reason =
                tr("No title found for this attribute type.");
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT("");
            return;
        }

        // Save content
        html_content[attribute] = QString("%1%2%3")
            .arg(html_pre,
                m_AttributeGUITitles[attribute],
                html_post);
    }

    // Initialize header image
    m_HeaderImage_Attributes = QImage(m_AttributesTotalWidth,
        m_HeaderHeight,
        QImage::Format_RGB32);
    m_HeaderImage_Attributes.fill(m_CanvasColor);
    QPainter painter(&m_HeaderImage_Attributes);

    // Render task text
    int pos = 0;
    for (const Attributes attribute : qAsConst(m_VisibleAttributes))
    {
        // Render text
        painter.save();
        QTextDocument text;
        text.setHtml(html_content[attribute]);
        text.setDocumentMargin(0.0);
        text.setDefaultFont(m_DefaultFont);
        const int effective_width =
            m_AttributeWidths[attribute] - 2*m_AttributePadding;
        text.setTextWidth(effective_width);
        painter.translate(pos + m_AttributePadding, m_RowPadding);
        text.drawContents(&painter);
        painter.restore();

        // Vertical line
        pos += m_AttributeWidths[attribute];
        painter.drawLine(pos - 1, 0, pos - 1, m_HeaderHeight);
    }

    // Horizontal line
    painter.drawLine(0,
        m_HeaderHeight - 1,
        m_AttributesTotalWidth,
        m_HeaderHeight - 1);

    // Done
    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Calculate row height
int ProjectEditor::GetRowImageHeight(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(QString::number(mcIndex)));

    // Check if row is a task item or a task group
    int row_height = 0;
    if (m_VisibleIDTypes[mcIndex] == AllTaskGroups::ElementType_TaskID)
    {
        row_height = GetRowImageHeight_TaskItem(mcIndex) + 2*m_RowPadding;
    } else if (m_VisibleIDTypes[mcIndex] == AllTaskGroups::ElementType_GroupID)
    {
        row_height = GetRowImageHeight_TaskGroup(mcIndex) + 2*m_RowPadding;
    } else
    {
        // Error
        const QString reason = tr("Invalid element type at index %1")
            .arg(QString::number(mcIndex));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return 0;
    }

    // Done
    CALL_OUT("");
    return row_height;
}



///////////////////////////////////////////////////////////////////////////////
// Calculate row height: task item
int ProjectEditor::GetRowImageHeight_TaskItem(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(QString::number(mcIndex)));

    // Check if height is cached
    const int task_id = m_VisibleIDs[mcIndex];
    if (m_TaskIDToRowImageHeight.contains(task_id))
    {
        CALL_OUT("");
        return m_TaskIDToRowImageHeight[task_id];
    }

    // Determine row height
    int row_height = 0;

    // Determine row content
    for (const Attributes attribute : qAsConst(m_VisibleAttributes))
    {
        int this_height = 0;
        if (attribute == Attribute_GanttChart)
        {
            this_height = m_GanttChart_BarHeight;
        } else
        {
            // Abbrevation
            const int task_id = m_VisibleIDs[mcIndex];

            // Get data
            const QPair < QStringList, QList < int > > content =
                GetTaskContent(task_id, attribute);
            const QStringList & content_html = content.first;

            // Indentation for title
            int indent = 0;
            if (attribute == Attribute_Title)
            {
                indent = m_VisibleIDIndentation[mcIndex] * m_IndentScale;
            }

            // Render existing rows
            for (int content_index = 0;
                 content_index < content_html.size();
                 content_index++)
            {
                QTextDocument text;
                text.setHtml(content_html[content_index]);
                text.setDocumentMargin(0.0);
                text.setDefaultFont(m_DefaultFont);
                const int effective_width = m_AttributeWidths[attribute]
                    - 2*m_AttributePadding - indent;
                text.setTextWidth(effective_width);
                this_height += text.size().height();
            }
        }

        // Determine total required height
        row_height = qMax(row_height, this_height);
    }

    // Store in cache
    m_TaskIDToRowImageHeight[task_id] = row_height;

    CALL_OUT("");
    return row_height;
}



///////////////////////////////////////////////////////////////////////////////
// Calculate row height: task group
int ProjectEditor::GetRowImageHeight_TaskGroup(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(QString::number(mcIndex)));

    // Check if height is cached
    const int group_id = m_VisibleIDs[mcIndex];
    if (m_GroupIDToRowImageHeight.contains(group_id))
    {
        CALL_OUT("");
        return m_GroupIDToRowImageHeight[group_id];
    }

    // Determine row height
    int row_height = 0;

    // Determine row content
    for (const Attributes attribute : qAsConst(m_VisibleAttributes))
    {
        int this_height = 0;
        if (attribute == Attribute_GanttChart)
        {
            this_height = m_GanttChart_BarHeight;
        } else
        {
            // Abbrevation
            const int group_id = m_VisibleIDs[mcIndex];

            // Get data
            const QPair < QStringList, QList < int > > content =
                GetGroupContent(group_id, attribute);
            const QStringList & content_html = content.first;

            // Indentation for title
            int indent = 0;
            if (attribute == Attribute_Title)
            {
                indent = m_VisibleIDIndentation[mcIndex] * m_IndentScale;
            }

            // Render existing rows
            for (int content_index = 0;
                 content_index < content_html.size();
                 content_index++)
            {
                QTextDocument text;
                text.setHtml(content_html[content_index]);
                text.setDocumentMargin(0.0);
                text.setDefaultFont(m_DefaultFont);
                const int effective_width = m_AttributeWidths[attribute]
                    - 2*m_AttributePadding - indent;
                text.setTextWidth(effective_width);
                this_height += text.size().height();
            }
        }

        // Determine total required height
        row_height = qMax(row_height, this_height);
    }

    // Store in cache
    m_GroupIDToRowImageHeight[group_id] = row_height;

    CALL_OUT("");
    return row_height;
}



///////////////////////////////////////////////////////////////////////////////
// Task items images
void ProjectEditor::CreateRowImage_TaskItem_Attributes(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(QString::number(mcIndex)));

    // Make sure schedule is up to date
    Project * p = Project::Instance();
    p -> UpdateSchedule();

    // Create image
    const int task_id = m_VisibleIDs[mcIndex];
    const int row_height = GetRowImageHeight(mcIndex);
    QImage image(m_AttributesTotalWidth, row_height, QImage::Format_RGB32);
    image.fill(m_BackgroundColors[mcIndex % 2]);
    QPainter painter(&image);
    if (m_SelectedTaskIDs.contains(task_id))
    {
        painter.save();
        painter.setOpacity(m_SelectedOpacity);
        painter.fillRect(0, 0, image.width(), image.height(),
            m_SelectedIndexColor);
        painter.restore();
    }
    if (m_HoveredIDType == AllTaskGroups::ElementType_TaskID &&
        m_HoveredID == task_id)
    {
        painter.save();
        painter.setOpacity(m_HoverOpacity);
        painter.fillRect(0, 0, image.width(), image.height(), Qt::blue);
        painter.restore();
    }

    // Reset any clickable actions for this cell
    m_HoveredCell_ActionXMin.clear();
    m_HoveredCell_ActionXMax.clear();
    m_HoveredCell_ActionYMin.clear();
    m_HoveredCell_ActionYMax.clear();
    m_HoveredCell_ActionType.clear();
    m_HoveredCell_ActionData.clear();

    // Determine row content
    for (const Attributes attribute : qAsConst(m_VisibleAttributes))
    {
        switch (attribute)
        {
        case Attribute_Attachments:
            CreateRowImage_TaskItem_Attachments(&painter, mcIndex);
            break;

        case Attribute_Comments:
            CreateRowImage_TaskItem_Comments(&painter, mcIndex);
            break;

        case Attribute_CompletionStatus:
            CreateRowImage_TaskItem_CompletionStatus(&painter, mcIndex);
            break;

        case Attribute_CriticalPath:
            CreateRowImage_TaskItem_CriticalPath(&painter, mcIndex);
            break;

        case Attribute_Duration:
            CreateRowImage_TaskItem_Duration(&painter, mcIndex);
            break;

        case Attribute_FinishDate:
            CreateRowImage_TaskItem_FinishDate(&painter, mcIndex);
            break;

        case Attribute_GanttChart:
            // Gets rendered separately
            break;

        case Attribute_ID:
            CreateRowImage_TaskItem_ID(&painter, mcIndex);
            break;

        case Attribute_Predecessors:
            CreateRowImage_TaskItem_Predecessors(&painter, mcIndex);
            break;

        case Attribute_Resources:
            CreateRowImage_TaskItem_Resources(&painter, mcIndex);
            break;

        case Attribute_SlackCalendarDays:
            CreateRowImage_TaskItem_SlackCalendarDays(&painter, mcIndex);
            break;

        case Attribute_SlackWorkdays:
            CreateRowImage_TaskItem_SlackWorkdays(&painter, mcIndex);
            break;

        case Attribute_StartDate:
            CreateRowImage_TaskItem_StartDate(&painter, mcIndex);
            break;

        case Attribute_Successors:
            CreateRowImage_TaskItem_Successors(&painter, mcIndex);
            break;

        case Attribute_Title:
            CreateRowImage_TaskItem_Title(&painter, mcIndex);
            break;

        default:
        {
            // Error
            const QString reason =
                tr("Invalid attribute in visible attributes.");
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return;
        }
        }
    }

    // Vertical lines
    for (int attr_index = 0;
         attr_index < m_VisibleAttributes.size();
         attr_index++)
    {
        const int pos = m_VisibleAttributesRightCoordinates[attr_index];
        painter.drawLine(pos - 1, 0, pos - 1, row_height);
    }

    // Store image
    m_TaskItemIDToImage_Attributes[task_id] = image;

    CALL_OUT("");
    return;
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: attachments
void ProjectEditor::CreateRowImage_TaskItem_Attachments(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_Attachments);
    const QStringList & content_html = content.first;
    const QList < int > & content_data = content.second;

    // Render text
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_Attachments);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    const int effective_width =
        m_AttributeWidths[Attribute_Attachments] - 2*m_AttributePadding;
    int current_y = 0;
    for (int content_index = 0;
         content_index < content_html.size();
         content_index++)
    {
        QTextDocument text;
        text.setHtml(content_html[content_index]);
        text.setDocumentMargin(0.0);
        text.setDefaultFont(m_DefaultFont);
        text.setTextWidth(effective_width);
        const int current_dy = text.size().height();

        mpPainter -> save();
        mpPainter -> translate(pos + m_AttributePadding, current_y);
        text.drawContents(mpPainter);
        mpPainter -> restore();

        // Cell actions
        if (mcIndex == m_HoveredIndex &&
            m_HoveredAttribute == Attribute_Attachments)
        {
            if (m_HoveredCell_Y > current_y &&
                m_HoveredCell_Y < current_y + current_dy)
            {
                mpPainter -> save();
                mpPainter -> setOpacity(0.6);
                int current_x = effective_width
                    - m_ImagePlus.width() - m_ImageMinus.width();

                // "Minus"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImageMinus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImageMinus.height();
                m_HoveredCell_ActionType << CellAction_Subtract;
                m_HoveredCell_ActionData << content_data[content_index];
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImageMinus);
                current_x += m_ImageMinus.width();

                // "Plus"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
                m_HoveredCell_ActionType << CellAction_Add;
                m_HoveredCell_ActionData << -1;
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImagePlus);

                mpPainter -> restore();
            }
        }

        current_y += current_dy;
    }

    // Cell action if no content
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_Attachments &&
        content_html.isEmpty())
    {
        mpPainter -> save();
        mpPainter -> setOpacity(0.6);

        // "Plus"
        int current_x = effective_width - m_ImagePlus.width();
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
        m_HoveredCell_ActionType << CellAction_Add;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImagePlus);

        mpPainter -> restore();
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: comments
void ProjectEditor::CreateRowImage_TaskItem_Comments(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_Comments);
    const QStringList & content_html = content.first;
    const QList < int > & content_data = content.second;

    // Render text
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_Comments);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    const int effective_width =
        m_AttributeWidths[Attribute_Comments] - 2*m_AttributePadding;
    int current_y = 0;
    for (int content_index = 0;
         content_index < content_html.size();
         content_index++)
    {
        QTextDocument text;
        text.setHtml(content_html[content_index]);
        text.setDocumentMargin(0.0);
        text.setDefaultFont(m_DefaultFont);
        text.setTextWidth(effective_width);
        const int current_dy = text.size().height();

        mpPainter -> save();
        mpPainter -> translate(pos + m_AttributePadding, current_y);
        text.drawContents(mpPainter);
        mpPainter -> restore();

        if (mcIndex == m_HoveredIndex &&
            m_HoveredAttribute == Attribute_Comments)
        {
            if (m_HoveredCell_Y > current_y &&
                m_HoveredCell_Y < current_y + current_dy)
            {
                mpPainter -> save();
                mpPainter -> setOpacity(0.6);
                int current_x = effective_width
                    - m_ImagePlus.width() - m_ImageMinus.width()
                    - m_ImageEdit.width();

                // Edit
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImageEdit.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImageEdit.height();
                m_HoveredCell_ActionType << CellAction_Edit;
                m_HoveredCell_ActionData << content_data[content_index];
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImageEdit);
                current_x += m_ImageEdit.width();

                // "Minus"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImageMinus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImageMinus.height();
                m_HoveredCell_ActionType << CellAction_Subtract;
                m_HoveredCell_ActionData << content_data[content_index];
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImageMinus);
                current_x += m_ImageMinus.width();

                // "Plus"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
                m_HoveredCell_ActionType << CellAction_Add;
                m_HoveredCell_ActionData << -1;
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImagePlus);

                mpPainter -> restore();
            }
        }

        current_y += text.size().height();
    }

    // Cell action if no content
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_Comments &&
        content_html.isEmpty())
    {
        mpPainter -> save();
        mpPainter -> setOpacity(0.6);

        // "Plus"
        int current_x = effective_width - m_ImagePlus.width();
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
        m_HoveredCell_ActionType << CellAction_Add;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImagePlus);

        mpPainter -> restore();
    }

    CALL_OUT("");
}




///////////////////////////////////////////////////////////////////////////////
// Task items images: completion status
void ProjectEditor::CreateRowImage_TaskItem_CompletionStatus(
    QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_CompletionStatus);
    const QStringList & content_html = content.first;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_CompletionStatus] - 2*m_AttributePadding;
    text.setTextWidth(effective_width);

    mpPainter -> save();
    const int attr_index =
        m_VisibleAttributes.indexOf(Attribute_CompletionStatus);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    // Cell actions
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_CompletionStatus)
    {
        mpPainter -> save();
        mpPainter -> setOpacity(0.6);

        // Not started
        int current_x = effective_width
            - m_ImageRed.width() - m_ImageYellow.width()
            - m_ImageGreen.width();
        int current_y = 0;
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImageRed.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImageRed.height();
        m_HoveredCell_ActionType << CellAction_NotStarted;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImageRed);
        current_x += m_ImageRed.width();

        // Started
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImageYellow.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImageYellow.height();
        m_HoveredCell_ActionType << CellAction_Started;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImageYellow);
        current_x += m_ImageYellow.width();

        // Completed
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImageGreen.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImageGreen.height();
        m_HoveredCell_ActionType << CellAction_Completed;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImageGreen);

        mpPainter -> restore();
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: critical path
void ProjectEditor::CreateRowImage_TaskItem_CriticalPath(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_CriticalPath);
    const QStringList & content_html = content.first;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_CriticalPath] - 2*m_AttributePadding;
    text.setTextWidth(effective_width);

    mpPainter -> save();
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_CriticalPath);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: duration
void ProjectEditor::CreateRowImage_TaskItem_Duration(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mpPainter=..., mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_Duration);
    const QStringList & content_html = content.first;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_Duration] - 2*m_AttributePadding;
    text.setTextWidth(effective_width);

    mpPainter -> save();
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_Duration);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    // Cell actions
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_Duration)
    {
        // "Plus"
        int current_x = effective_width
            - m_ImagePlus.width() - m_ImageMinus.width();
        const int current_y = 0;
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
        m_HoveredCell_ActionType << CellAction_Add;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImagePlus);

        // "Minus"
        current_x += m_ImageMinus.width();
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImageMinus.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImageMinus.height();
        m_HoveredCell_ActionType << CellAction_Subtract;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImageMinus);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: finish date
void ProjectEditor::CreateRowImage_TaskItem_FinishDate(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_FinishDate);
    const QStringList & content_html = content.first;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_FinishDate] - 2*m_AttributePadding;
    text.setTextWidth(effective_width);

    mpPainter -> save();
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_FinishDate);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: reference
void ProjectEditor::CreateRowImage_TaskItem_ID(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mpPainter=..., mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_ID);
    const QStringList & content_html = content.first;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_ID] - 2*m_AttributePadding;
    text.setTextWidth(effective_width);

    mpPainter -> save();
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_ID);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    // Cell actions
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_ID)
    {
        // "Edit"
        const int current_x = effective_width
            - m_ImageEdit.width();
        const int current_y = 0;
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImageEdit.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImageEdit.height();
        m_HoveredCell_ActionType << CellAction_Edit;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImageEdit);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: predecessors
void ProjectEditor::CreateRowImage_TaskItem_Predecessors(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_Predecessors);
    const QStringList & content_html = content.first;
    const QList < int > & content_data = content.second;

    // Render text
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_Predecessors);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    const int effective_width =
        m_AttributeWidths[Attribute_Predecessors] - 2*m_AttributePadding;
    int current_y = 0;
    for (int content_index = 0;
         content_index < content_html.size();
         content_index++)
    {
        QTextDocument text;
        text.setHtml(content_html[content_index]);
        text.setDocumentMargin(0.0);
        text.setDefaultFont(m_DefaultFont);
        text.setTextWidth(effective_width);
        const int current_dy = text.size().height();

        mpPainter -> save();
        mpPainter -> translate(pos + m_AttributePadding, current_y);
        text.drawContents(mpPainter);
        mpPainter -> restore();

        if (mcIndex == m_HoveredIndex &&
            m_HoveredAttribute == Attribute_Predecessors)
        {
            if (m_HoveredCell_Y > current_y &&
                m_HoveredCell_Y < current_y + current_dy)
            {
                mpPainter -> save();
                mpPainter -> setOpacity(0.6);
                int current_x = effective_width
                    - m_ImagePlus.width() - m_ImageMinus.width()
                    - m_ImageEdit.width();

                // "Edit"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImageEdit.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImageEdit.height();
                m_HoveredCell_ActionType << CellAction_Edit;
                m_HoveredCell_ActionData << content_data[content_index];
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImageEdit);
                current_x += m_ImageEdit.width();

                // "Minus"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImageMinus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImageMinus.height();
                m_HoveredCell_ActionType << CellAction_Subtract;
                m_HoveredCell_ActionData << content_data[content_index];
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImageMinus);
                current_x += m_ImageMinus.width();

                // "Plus"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
                m_HoveredCell_ActionType << CellAction_Add;
                m_HoveredCell_ActionData << -1;
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImagePlus);

                mpPainter -> restore();
            }
        }

        current_y += current_dy;
    }

    // Cell action if no content
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_Predecessors &&
        content_html.isEmpty())
    {
        mpPainter -> save();
        mpPainter -> setOpacity(0.6);

        // "Plus"
        int current_x = effective_width - m_ImagePlus.width();
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
        m_HoveredCell_ActionType << CellAction_Add;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImagePlus);

        mpPainter -> restore();
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: resources
void ProjectEditor::CreateRowImage_TaskItem_Resources(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_Resources);
    const QStringList & content_html = content.first;
    const QList < int > & content_data = content.second;

    // Render text
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_Resources);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    const int effective_width =
        m_AttributeWidths[Attribute_Resources] - 2*m_AttributePadding;
    int current_y = 0;
    for (int content_index = 0;
         content_index < content_html.size();
         content_index++)
    {
        QTextDocument text;
        text.setHtml(content_html[content_index]);
        text.setDocumentMargin(0.0);
        text.setDefaultFont(m_DefaultFont);
        text.setTextWidth(effective_width);
        const int current_dy = text.size().height();

        mpPainter -> save();
        mpPainter -> translate(pos + m_AttributePadding, current_y);
        text.drawContents(mpPainter);
        mpPainter -> restore();

        // Cell actions
        if (mcIndex == m_HoveredIndex &&
            m_HoveredAttribute == Attribute_Resources)
        {
            if (m_HoveredCell_Y > current_y &&
                m_HoveredCell_Y < current_y + current_dy)
            {
                mpPainter -> save();
                mpPainter -> setOpacity(0.6);
                int current_x = effective_width
                    - m_ImagePlus.width() - m_ImageMinus.width();

                // "Minus"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImageMinus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImageMinus.height();
                m_HoveredCell_ActionType << CellAction_Subtract;
                m_HoveredCell_ActionData << content_data[content_index];
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImageMinus);
                current_x += m_ImageMinus.width();

                // "Plus"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
                m_HoveredCell_ActionType << CellAction_Add;
                m_HoveredCell_ActionData << -1;
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImagePlus);

                mpPainter -> restore();
            }
        }

        current_y += current_dy;
    }

    // Cell action if no content
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_Resources &&
        content_html.isEmpty())
    {
        mpPainter -> save();
        mpPainter -> setOpacity(0.6);

        // "Plus"
        int current_x = effective_width - m_ImagePlus.width();
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax <<
            current_x + m_ImagePlus.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax <<
            current_y + m_ImagePlus.height();
        m_HoveredCell_ActionType << CellAction_Add;
        m_HoveredCell_ActionData << 0;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImagePlus);

        mpPainter -> restore();
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: slack (calendar days)
void ProjectEditor::CreateRowImage_TaskItem_SlackCalendarDays(
    QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_SlackCalendarDays);
    const QStringList & content_html = content.first;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_SlackCalendarDays] - 2*m_AttributePadding;
    text.setTextWidth(effective_width);

    mpPainter -> save();
    const int attr_index =
        m_VisibleAttributes.indexOf(Attribute_SlackCalendarDays);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: slack (workdays)
void ProjectEditor::CreateRowImage_TaskItem_SlackWorkdays(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_SlackWorkdays);
    const QStringList & content_html = content.first;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_SlackWorkdays] - 2*m_AttributePadding;
    text.setTextWidth(effective_width);

    mpPainter -> save();
    const int attr_index =
        m_VisibleAttributes.indexOf(Attribute_SlackWorkdays);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: start date
void ProjectEditor::CreateRowImage_TaskItem_StartDate(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_StartDate);
    const QStringList & content_html = content.first;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_StartDate] - 2*m_AttributePadding;
    text.setTextWidth(effective_width);

    mpPainter -> save();
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_StartDate);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: successors
void ProjectEditor::CreateRowImage_TaskItem_Successors(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_Successors);
    const QStringList & content_html = content.first;
    const QList < int > & content_data = content.second;

    // Render text
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_Successors);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    const int effective_width =
        m_AttributeWidths[Attribute_Successors] - 2*m_AttributePadding;
    int current_y = 0;
    for (int content_index = 0;
         content_index < content_html.size();
         content_index++)
    {
        QTextDocument text;
        text.setHtml(content_html[content_index]);
        text.setDocumentMargin(0.0);
        text.setDefaultFont(m_DefaultFont);
        text.setTextWidth(effective_width);
        const int current_dy = text.size().height();

        mpPainter -> save();
        mpPainter -> translate(pos + m_AttributePadding, current_y);
        text.drawContents(mpPainter);
        mpPainter -> restore();

        if (mcIndex == m_HoveredIndex &&
            m_HoveredAttribute == Attribute_Successors)
        {
            if (m_HoveredCell_Y > current_y &&
                m_HoveredCell_Y < current_y + current_dy)
            {
                mpPainter -> save();
                mpPainter -> setOpacity(0.6);
                int current_x = effective_width
                    - m_ImagePlus.width() - m_ImageMinus.width()
                    - m_ImageEdit.width();

                // "Edit"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImageEdit.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImageEdit.height();
                m_HoveredCell_ActionType << CellAction_Edit;
                m_HoveredCell_ActionData << content_data[content_index];
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImageEdit);
                current_x += m_ImageEdit.width();

                // "Minus"
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImageMinus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImageMinus.height();
                m_HoveredCell_ActionType << CellAction_Subtract;
                m_HoveredCell_ActionData << content_data[content_index];
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImageMinus);

                // "Plus"
                current_x += m_ImageMinus.width();
                m_HoveredCell_ActionXMin << current_x;
                m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
                m_HoveredCell_ActionYMin << current_y;
                m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
                m_HoveredCell_ActionType << CellAction_Add;
                m_HoveredCell_ActionData << -1;
                mpPainter -> drawImage(pos + m_AttributePadding + current_x,
                    m_RowPadding + current_y,
                    m_ImagePlus);

                mpPainter -> restore();
            }
        }

        current_y += current_dy;
    }

    // Cell action if no content
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_Successors &&
        content_data.isEmpty())
    {
        mpPainter -> save();
        mpPainter -> setOpacity(0.6);

        // "Plus"
        int current_x = effective_width - m_ImagePlus.width();
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImagePlus.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImagePlus.height();
        m_HoveredCell_ActionType << CellAction_Add;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImagePlus);

        mpPainter -> restore();
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task items images: title
void ProjectEditor::CreateRowImage_TaskItem_Title(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mpPainter=..., mcIndex=%1, mcrTaskInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int task_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetTaskContent(task_id, Attribute_Title);
    const QStringList & content_html = content.first;

    // Indentation
    const int indent = m_VisibleIDIndentation[mcIndex] * m_IndentScale;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_Title] - 2*m_AttributePadding;
    const int effective_width_text = effective_width - indent;
    text.setTextWidth(effective_width_text);

    mpPainter -> save();
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_Title);
    int pos = m_VisibleAttributesLeftCoordinates[attr_index] + indent;
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    // Cell actions
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_Title)
    {
        pos = m_VisibleAttributesLeftCoordinates[attr_index];

        // "Edit"
        const int current_x = effective_width
            - m_ImageEdit.width();
        const int current_y = 0;
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImageEdit.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImageEdit.height();
        m_HoveredCell_ActionType << CellAction_Edit;
        m_HoveredCell_ActionData << 0;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImageEdit);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task group images
void ProjectEditor::CreateRowImage_TaskGroup_Attributes(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(QString::number(mcIndex)));

    // Make sure schedule is up to date
    Project * p = Project::Instance();
    p -> UpdateSchedule();

    // Create image
    const int group_id = m_VisibleIDs[mcIndex];
    const int row_height = GetRowImageHeight(mcIndex);
    QImage image(m_AttributesTotalWidth, row_height, QImage::Format_RGB32);
    image.fill(m_BackgroundColors[mcIndex % 2]);
    QPainter painter(&image);
    if (m_SelectedGroupIDs.contains(group_id))
    {
        painter.save();
        painter.setOpacity(m_SelectedOpacity);
        painter.fillRect(0, 0, image.width(), image.height(),
            m_SelectedIndexColor);
        painter.restore();
    }
    if (m_HoveredIDType == AllTaskGroups::ElementType_GroupID &&
        m_HoveredID == group_id)
    {
        painter.save();
        painter.setOpacity(m_HoverOpacity);
        painter.fillRect(0, 0, image.width(), image.height(), Qt::blue);
        painter.restore();
    }

    // Reset any clickable actions for this cell
    m_HoveredCell_ActionXMin.clear();
    m_HoveredCell_ActionXMax.clear();
    m_HoveredCell_ActionYMin.clear();
    m_HoveredCell_ActionYMax.clear();
    m_HoveredCell_ActionType.clear();
    m_HoveredCell_ActionData.clear();

    // Determine row content
    for (const Attributes attribute : qAsConst(m_VisibleAttributes))
    {
        switch (attribute)
        {
        case Attribute_Attachments:
            // Falls through
        case Attribute_Comments:
            break;

        case Attribute_CompletionStatus:
            CreateRowImage_TaskGroup_CompletionStatus(&painter, mcIndex);
            break;

        case Attribute_CriticalPath:
            // Falls through
        case Attribute_Duration:
            // Falls through
        case Attribute_FinishDate:
            // Falls through
        case Attribute_GanttChart:
            // Falls through
        case Attribute_ID:
            // Falls through
        case Attribute_Predecessors:
            // Falls through
        case Attribute_Resources:
            // Falls through
        case Attribute_SlackCalendarDays:
            // Falls through
        case Attribute_SlackWorkdays:
            // Falls through
        case Attribute_StartDate:
            // Falls through
        case Attribute_Successors:
            break;

        case Attribute_Title:
            CreateRowImage_TaskGroup_Title(&painter, mcIndex);
            break;

        default:
        {
            // Error
            const QString reason =
                tr("Invalid attribute in visible attributes.");
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return;
        }
        }
    }

    // Vertical lines
    for (int attr_index = 0;
         attr_index < m_VisibleAttributes.size();
         attr_index++)
    {
        const int pos = m_VisibleAttributesRightCoordinates[attr_index];
        painter.drawLine(pos - 1, 0, pos - 1, row_height);
    }

    // Store image
    m_TaskGroupIDToImage_Attributes[group_id] = image;

    CALL_OUT("");
    return;
}



///////////////////////////////////////////////////////////////////////////////
// Task group images: completion status
void ProjectEditor::CreateRowImage_TaskGroup_CompletionStatus(
    QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrGroupInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int group_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetGroupContent(group_id, Attribute_CompletionStatus);
    const QStringList & content_html = content.first;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_CompletionStatus] - 2*m_AttributePadding;
    text.setTextWidth(effective_width);

    mpPainter -> save();
    const int attr_index =
        m_VisibleAttributes.indexOf(Attribute_CompletionStatus);
    const int pos = m_VisibleAttributesLeftCoordinates[attr_index];
    mpPainter -> translate(pos + m_AttributePadding, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task group images: title
void ProjectEditor::CreateRowImage_TaskGroup_Title(QPainter * mpPainter,
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1, mcrGroupInfo=...")
        .arg(QString::number(mcIndex)));

    // Abbrevation
    const int group_id = m_VisibleIDs[mcIndex];

    // Get data
    const QPair < QStringList, QList < int > > content =
        GetGroupContent(group_id, Attribute_Title);
    const QStringList & content_html = content.first;

    // Structure of this...
    // (AttributePadding)
    //   (Indent) (TriangleWidth) (TrianglePostOffset) (Text)
    // (AttributePadding)
    // The whole thing is exactly (AttributeWidth[Title]) wide.

    // Indentation
    const int indent = m_VisibleIDIndentation[mcIndex] * m_IndentScale;

    // Render triangle
    const int attr_index = m_VisibleAttributes.indexOf(Attribute_Title);
    int pos = m_VisibleAttributesLeftCoordinates[attr_index]
        + m_AttributePadding + indent;
    // (Qt6 does not support UTF-8 characters, otherwise
    // &#25BC; and &#25B6; would do the trick)
    QPainterPath triangle;
    if (m_ExpandedTaskGroups.contains(group_id))
    {
        triangle.moveTo(pos, m_TrianglePadding);
        triangle.lineTo(pos + m_TriangleWidth, m_TrianglePadding);
        triangle.lineTo(pos + m_TriangleWidth/2,
            m_TrianglePadding + m_TriangleHeight);
        triangle.lineTo(pos, m_TrianglePadding);
    } else
    {
        triangle.moveTo(pos, m_TrianglePadding);
        triangle.lineTo(pos, m_TrianglePadding + m_TriangleHeight);
        triangle.lineTo(pos + m_TriangleWidth,
            m_TrianglePadding + m_TriangleHeight/2);
        triangle.lineTo(pos, m_TrianglePadding);
    }
    mpPainter -> fillPath(triangle, Qt::black);
    pos += m_TriangleWidth + m_TrianglePostOffset;

    // Render text
    QTextDocument text;
    text.setHtml(content_html.first());
    text.setDocumentMargin(0.0);
    text.setDefaultFont(m_DefaultFont);
    const int effective_width =
        m_AttributeWidths[Attribute_Title] - 2*m_AttributePadding;
    const int effective_width_text =
        effective_width - indent - m_TriangleWidth - m_TrianglePostOffset;
    text.setTextWidth(effective_width_text);

    mpPainter -> save();
    mpPainter -> translate(pos, 0);
    text.drawContents(mpPainter);
    mpPainter -> restore();

    // Cell actions
    if (mcIndex == m_HoveredIndex &&
        m_HoveredAttribute == Attribute_Title)
    {
        pos = m_VisibleAttributesLeftCoordinates[attr_index];

        // "Edit"
        const int current_x = effective_width
            - m_ImageEdit.width();
        const int current_y = 0;
        m_HoveredCell_ActionXMin << current_x;
        m_HoveredCell_ActionXMax << current_x + m_ImageEdit.width();
        m_HoveredCell_ActionYMin << current_y;
        m_HoveredCell_ActionYMax << current_y + m_ImageEdit.height();
        m_HoveredCell_ActionType << CellAction_Edit;
        m_HoveredCell_ActionData << -1;
        mpPainter -> drawImage(pos + m_AttributePadding + current_x,
            m_RowPadding + current_y,
            m_ImageEdit);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Get image, regardless of type
QImage ProjectEditor::GetRowImage_Attributes(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(QString::number(mcIndex)));

    // Private method - no checks

    if (m_VisibleIDTypes[mcIndex] ==
        AllTaskGroups::ElementType_TaskID)
    {
        const int task_id = m_VisibleIDs[mcIndex];
        if (!m_TaskItemIDToImage_Attributes.contains(task_id))
        {
            CreateRowImage_TaskItem_Attributes(mcIndex);
        }
        CALL_OUT("");
        return m_TaskItemIDToImage_Attributes[task_id];
    } else if (m_VisibleIDTypes[mcIndex] ==
        AllTaskGroups::ElementType_GroupID)
    {
        const int group_id = m_VisibleIDs[mcIndex];
        if (!m_TaskGroupIDToImage_Attributes.contains(group_id))
        {
            CreateRowImage_TaskGroup_Attributes(mcIndex);
        }
        CALL_OUT("");
        return m_TaskGroupIDToImage_Attributes[group_id];
    } else
    {
        // Error
        const QString reason = tr("Unknown index type.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return QImage();
    }
}



///////////////////////////////////////////////////////////////////////////////
// Get a resource name, existing or new
int ProjectEditor::SelectResource() const
{
    CALL_IN("");

    QDialog * dialog = new QDialog();
    QHBoxLayout * layout = new QHBoxLayout();
    dialog -> setLayout(layout);

    QLabel * l_name = new QLabel(tr("Resource name:"));
    layout -> addWidget(l_name);

    AllResources * ar = AllResources::Instance();
    QList < QString > resource_names;
    const QList < int > all_resource_ids = ar -> GetAllIDs();
    for (const int resource_id : all_resource_ids)
    {
        const QHash < AllResources::Information, QString > resource_info =
            ar -> GetInformation(resource_id);
        resource_names << resource_info[AllResources::Information_Name];
    }
    AutocompletionLineEdit * name = new AutocompletionLineEdit(resource_names);
    connect (name, SIGNAL(ValueEntered(const QString)),
        dialog, SLOT(accept()));
    layout -> addWidget(name);

    const int success = dialog -> exec();
    int resource_id = AllResources::INVALID_ID;
    if (success == QDialog::Accepted)
    {
        const QString resource_name = name -> GetValue();
        resource_id = ar -> GetIDFromName(resource_name);
        if (resource_id == AllResources::INVALID_ID)
        {
            // New resource
            resource_id = ar -> Create(resource_name);
        }
    }

    // Clean up
    delete dialog;

    // Done
    CALL_OUT("");
    return resource_id;
}



///////////////////////////////////////////////////////////////////////////////
// Get new group/task name
QString ProjectEditor::GetName(const QString mcTitle) const
{
    CALL_IN(QString("mcTitle=\"%1\"")
        .arg(mcTitle));

    QDialog * dialog = new QDialog();
    QGridLayout * layout = new QGridLayout();
    dialog -> setLayout(layout);
    int row = 0;

    // Set title
    dialog -> setWindowTitle(mcTitle);

    // Title
    QLabel * l_title = new QLabel(tr("Title"));
    layout -> addWidget(l_title, row, 0, Qt::AlignTop);
    QLineEdit * title = new QLineEdit();
    layout -> addWidget(title, row, 1);
    row++;

    // Bottom row: ok and cancel
    QHBoxLayout * bottom_layout = new QHBoxLayout();
    layout -> addLayout(bottom_layout, row, 0, 1, 2);
    bottom_layout -> addStretch(1);
    QPushButton * ok = new QPushButton(tr("Ok"));
    ok -> setFixedWidth(100);
    connect (ok, SIGNAL(clicked()),
        dialog, SLOT(accept()));
    bottom_layout -> addWidget(ok);
    QPushButton * cancel = new QPushButton(tr("Cancel"));
    cancel -> setFixedWidth(100);
    connect (cancel, SIGNAL(clicked()),
        dialog, SLOT(reject()));
    bottom_layout -> addWidget(cancel);

    // Execute
    const int success = dialog -> exec();
    QString title_text = title -> text().trimmed();

    // Done
    delete dialog;

    if (success != QDialog::Accepted)
    {
        CALL_OUT("");
        return QString();
    }

    CALL_OUT("");
    return title_text;
}



///////////////////////////////////////////////////////////////////////////////
// Information for a comment has changed
void ProjectEditor::CommentChanged(const int mcCommentID)
{
    CALL_IN(QString("mcCommentID=%1")
        .arg(QString::number(mcCommentID)));

    // Are we actually showing comments?
    if (!m_VisibleAttributes.contains(Attribute_Comments))
    {
        // Nope - nothing to do.
        CALL_OUT("");
        return;
    }

    // No problems if the comment does not have a parent task
    // (happens whenever a comment is just being created)
    AllTaskItems * at = AllTaskItems::Instance();
    const int task_id = at -> GetTaskIDForCommentID(mcCommentID);
    if (task_id == AllTaskItems::INVALID_ID)
    {
        CALL_OUT("");
        return;
    }

    // Update affected task item
    m_TaskItemIDToImage_Attributes.remove(task_id);
    m_TaskIDToRowImageHeight.remove(task_id);

    // Update visuals
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Information for a resource has changed
void ProjectEditor::ResourceChanged(const int mcResourceID)
{
    CALL_IN(QString("mcResourceID=%1")
        .arg(QString::number(mcResourceID)));

    // Are we actually showing resources?
    if (!m_VisibleAttributes.contains(Attribute_Resources))
    {
        // Nope - nothing to do.
        CALL_OUT("");
        return;
    }

    // Check affected task items
    AllTaskItems * at = AllTaskItems::Instance();
    const QList < int > task_ids = at -> GetTaskIDsForResourceID(mcResourceID);
    for (const int task_id : task_ids)
    {
        m_TaskItemIDToImage_Attributes.remove(task_id);
        m_TaskIDToRowImageHeight.remove(task_id);
    }

    // Update visuals
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Information for an attachment has changed
void ProjectEditor::AttachmentChanged(const int mcAttachmentID)
{
    CALL_IN(QString("mcAttachmentID=%1")
        .arg(QString::number(mcAttachmentID)));

    // Are we actually showing attachments?
    if (!m_VisibleAttributes.contains(Attribute_Attachments))
    {
        // Nope - nothing to do.
        CALL_OUT("");
        return;
    }

    // Check affected task items
    AllTaskItems * at = AllTaskItems::Instance();
    const int task_id = at -> GetTaskIDForAttachmentID(mcAttachmentID);
    m_TaskItemIDToImage_Attributes.remove(task_id);
    m_TaskIDToRowImageHeight.remove(task_id);

    // Update visuals
    update();

    CALL_OUT("");
}



// =========================================================== GUI: Gantt Chart



///////////////////////////////////////////////////////////////////////////////
// Initialize Gantt chart pieces of information
void ProjectEditor::Initialize_GanttChart()
{
    CALL_IN("");

    // Dimensions
    m_GanttChart_BarNorthPadding = 6;
    m_GanttChart_BarWestPadding = 10;
    m_GanttChart_BarHeight = 10;
    m_GanttChart_BarMilestoneWidth = 10;
    m_GanttChart_HeaderLineHeight = m_DefaultFont.pixelSize() + 1;

    // Colors
    m_GanttBarColor = QColor(128,128,200);
    m_GanttCriticalPathColor = QColor(200,128,128);
    m_GanttHolidayBackgroundColor = QColor(220,220,220);
    m_TodayOpacity = 0.2;
    m_TodayColor = QColor(255,0,0);

    // Start date
    m_GanttChart_StartDate = QDate::currentDate();
    m_GanttChart_StartDateIsLocked = false;

    // Scale
    m_GanttChart_Scale = 20.;

    // Current date (today)
    m_GanttChart_CurrentDate = QDate::currentDate();

    // Header image (Gantt chart)
    m_HeaderImage_GanttChart = QImage();

    // Row height (Gantt Chart)

    // Row images (Gantt chart)
    m_TaskItemIDToImage_GanttChart.clear();
    m_TaskGroupIDToImage_GanttChart.clear();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Get start date for Gantt chart
QDate ProjectEditor::GetGanttChartStartDate() const
{
    CALL_IN("");
    CALL_OUT("");
    return m_GanttChart_StartDate;
}



///////////////////////////////////////////////////////////////////////////////
// Set start date for Gantt chart
void ProjectEditor::SetGanttChartStartDate(const QDate mcNewStartDate)
{
    CALL_IN(QString("mcNewStartDate=\"%1\"")
        .arg(mcNewStartDate.toString("yyyy-mm-dd")));

    if (!mcNewStartDate.isValid())
    {
        const QString reason = tr("Invalid new start date.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Check if there actually is an update
    if (mcNewStartDate == m_GanttChart_StartDate)
    {
        // Nope.
        CALL_OUT("");
        return;
    }

    // Set new start date
    m_GanttChart_StartDate = mcNewStartDate;
    m_TaskItemIDToImage_GanttChart.clear();
    m_TaskGroupIDToImage_GanttChart.clear();
    m_HeaderImage_GanttChart = QImage();
    update();

    // Let the outside world know
    emit GanttChartStartDateChanged(mcNewStartDate);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Set if Gantt chart start date is locked (while scrolling with mouse)
void ProjectEditor::SetGanttChartStartDateLocked(const bool mcIsLocked)
{
    CALL_IN(QString("mcIsLocked=%1")
        .arg(mcIsLocked ? "true" : "false"));

    m_GanttChart_StartDateIsLocked = mcIsLocked;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Get scale parameter for Gantt chart
double ProjectEditor::GetGanttChartScale() const
{
    CALL_IN("");
    CALL_OUT("");
    return m_GanttChart_Scale;
}



///////////////////////////////////////////////////////////////////////////////
// Set scale for Gantt chart
void ProjectEditor::SetGanttChartScale(const double mcNewScale)
{
    CALL_IN(QString("mcNewScale=%1")
        .arg(QString::number(mcNewScale)));

    // Check if within range
    if (mcNewScale < 1. ||
        mcNewScale > 50.)
    {
        const QString reason = tr("Invalid new scale value %1.")
            .arg(QString::number(mcNewScale));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Check if the value is new anyway
    if (fabs(mcNewScale - m_GanttChart_Scale) < 0.01)
    {
        // Nope.
        return;
    }

    // Set new scale
    m_GanttChart_Scale = mcNewScale;

    // Some things need to be updated
    m_HeaderImage_GanttChart = QImage();
    m_TaskItemIDToImage_GanttChart.clear();
    m_TaskGroupIDToImage_GanttChart.clear();
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Effective display format for Gantt chart header
ProjectEditor::AttributeDisplayFormat
    ProjectEditor::GetEffectiveGanttChartDisplayFormat() const
{
    CALL_IN("");

    AttributeDisplayFormat gantt_header =
        m_AttributeDisplayFormat[Attribute_GanttChart];
    if (gantt_header == AttributeDisplayFormat_GanttAutomatic)
    {
        if (m_GanttChart_Scale >= 20)
        {
            gantt_header = AttributeDisplayFormat_GanttDays;
        } else if (m_GanttChart_Scale >= 10)
        {
            gantt_header = AttributeDisplayFormat_GanttWeeks;
        } else if (m_GanttChart_Scale >= 5)
        {
            gantt_header = AttributeDisplayFormat_GanttMonths;
        } else
        {
            gantt_header = AttributeDisplayFormat_GanttYears;
        }
    }

    CALL_OUT("");
    return gantt_header;
}



///////////////////////////////////////////////////////////////////////////////
// Check if current date is still today
void ProjectEditor::CheckIfCurrentDateChanged()
{
    CALL_IN("");

    if (m_GanttChart_CurrentDate == QDate::currentDate())
    {
        // All good.
        CALL_OUT("");
        return;
    }

    // Current date changed. Redo all Gantt-Chart-related images
    m_GanttChart_CurrentDate = QDate::currentDate();
    m_HeaderImage_GanttChart = QImage();
    m_TaskItemIDToImage_GanttChart.clear();
    m_TaskGroupIDToImage_GanttChart.clear();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Calculate header height (Gantt chart)
int ProjectEditor::CalculateMinimumHeaderHeight_GanttChart()
{
    CALL_IN("");

    // If invisible
    if (!m_VisibleAttributes.contains(Attribute_GanttChart))
    {
        CALL_OUT("");
        return 0;
    }

    // Gantt chart
    const AttributeDisplayFormat gantt_header =
        GetEffectiveGanttChartDisplayFormat();
    int gantt_header_lines = 0;
    switch (gantt_header)
    {
    case AttributeDisplayFormat_GanttAutomatic:
        // Can't happen per above
        break;

    case AttributeDisplayFormat_GanttDays:
        // Two lines
        gantt_header_lines = 2;
        break;

    case AttributeDisplayFormat_GanttWeekdays:
        // Three lines
        gantt_header_lines = 3;
        break;

    case AttributeDisplayFormat_GanttWeeks:
        // Falls through
    case AttributeDisplayFormat_GanttMonths:
        // Falls through
    case AttributeDisplayFormat_GanttYears:
        // Single line
        gantt_header_lines = 1;
        break;

    default:
    {
        // Error
        const QString reason = tr("Unknown Gantt header format.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return 0;
    }
    }

    CALL_OUT("");
    return gantt_header_lines * m_GanttChart_HeaderLineHeight;
}



///////////////////////////////////////////////////////////////////////////////
// Header image: Gantt chart
void ProjectEditor::UpdateHeaderImage_GanttChart()
{
    CALL_IN("");

    // Private method - no checks

    // Nothing to do it not visible
    if (!m_VisibleAttributes.contains(Attribute_GanttChart))
    {
        CALL_OUT("");
        return;
    }

    // Some weekday stuff
    static QHash < int, QString > weekday_short;
    static QHash < int, QString > weekday_mid;
    if (weekday_short.isEmpty())
    {
        weekday_short[1] = tr("M");
        weekday_short[2] = tr("T");
        weekday_short[3] = tr("W");
        weekday_short[4] = tr("T");
        weekday_short[5] = tr("F");
        weekday_short[6] = tr("S");
        weekday_short[7] = tr("S");

        weekday_mid[1] = tr("Mon");
        weekday_mid[2] = tr("Tue");
        weekday_mid[3] = tr("Wed");
        weekday_mid[4] = tr("Thu");
        weekday_mid[5] = tr("Fri");
        weekday_mid[6] = tr("Sat");
        weekday_mid[7] = tr("Sun");
    }

    // Gantt chart
    const AttributeDisplayFormat gantt_header =
        GetEffectiveGanttChartDisplayFormat();

    // Initialize header image
    m_HeaderImage_GanttChart = QImage(m_AttributeWidths[Attribute_GanttChart],
        m_HeaderHeight,
        QImage::Format_RGB32);
    m_HeaderImage_GanttChart.fill(m_CanvasColor);
    QPainter painter(&m_HeaderImage_GanttChart);

    // Background color
    QDate date = m_GanttChart_StartDate;
    double gantt_pos = 0;
    if (gantt_header == AttributeDisplayFormat_GanttDays ||
        gantt_header == AttributeDisplayFormat_GanttWeekdays)
    {
        Calendar * c = Calendar::Instance();
        while (gantt_pos < m_AttributeWidths[Attribute_GanttChart])
        {
            if (date == m_GanttChart_CurrentDate)
            {
                // "Today" marker
                // Using ceil() avoids having gaps between days due to
                // rounding.
                // Using HeaderHeight ensures the background extends to the
                // bottom of the header.
                painter.save();
                painter.setOpacity(m_TodayOpacity);
                painter.fillRect(m_AttributePadding + ceil(gantt_pos),
                    m_RowPadding + m_GanttChart_HeaderLineHeight,
                    ceil(m_GanttChart_Scale),
                    m_HeaderHeight,
                    m_TodayColor);
                painter.restore();
            } else if (!c -> IsWorkday(date))
            {
                painter.fillRect(m_AttributePadding + ceil(gantt_pos),
                    m_RowPadding + m_GanttChart_HeaderLineHeight,
                    ceil(m_GanttChart_Scale),
                    m_HeaderHeight,
                    m_GanttHolidayBackgroundColor);
            }
            date = date.addDays(1);
            gantt_pos += m_GanttChart_Scale;
        }
    }

    // Text
    date = m_GanttChart_StartDate;
    gantt_pos = 0;
    double gantt_increment = 0;
    bool is_start = true;
    while (gantt_pos < m_AttributeWidths[Attribute_GanttChart])
    {
        QString gantt_header_top;
        QString gantt_header_middle;
        QString gantt_header_bottom;
        switch (gantt_header)
        {
        case AttributeDisplayFormat_GanttAutomatic:
            // Can't happen per above
            gantt_increment = m_AttributeWidths[Attribute_GanttChart];
            break;

        case AttributeDisplayFormat_GanttDays:
        {
            if (is_start)
            {
                // Check if the name of the month will still fit
                if (date.daysInMonth() - date.day() > 5)
                {
                    gantt_header_top = date.toString("MMM yyyy");
                }
            } else if (date.day() == 1)
            {
                gantt_header_top = date.toString("MMM yyyy");
            }
            gantt_header_middle = date.toString("dd");
            date = date.addDays(1);
            gantt_increment = m_GanttChart_Scale;
            break;
        }

        case AttributeDisplayFormat_GanttWeekdays:
        {
            if (is_start)
            {
                // Check if the name of the month will still fit
                if (date.daysInMonth() - date.day() > 5)
                {
                    gantt_header_top = date.toString("MMM yyyy");
                }
            } else if (date.day() == 1)
            {
                gantt_header_top = date.toString("MMM yyyy");
            }
            gantt_header_middle = date.toString("dd");
            if (m_GanttChart_Scale > 30)
            {
                gantt_header_bottom = weekday_mid[date.dayOfWeek()];
            } else
            {
                gantt_header_bottom = weekday_short[date.dayOfWeek()];
            }
            date = date.addDays(1);
            gantt_increment = m_GanttChart_Scale;
            break;
        }

        case AttributeDisplayFormat_GanttWeeks:
        {
            const int remaining_days = 7 - date.dayOfWeek();
            if (is_start)
            {
                // Check if the name of the month will still fit
                if (remaining_days > 3)
                {
                    gantt_header_top = QString("CW %1")
                        .arg(date.weekNumber());
                }
            } else if (date.dayOfWeek() == 1)
            {
                gantt_header_top = QString("CW %1")
                    .arg(date.weekNumber());
            }
            date = date.addDays(remaining_days + 1);
            gantt_increment = m_GanttChart_Scale * (remaining_days + 1);
            break;
        }

        case AttributeDisplayFormat_GanttMonths:
        {
            const int remaining_days = date.daysInMonth() - date.day();
            if (is_start)
            {
                // Check if the name of the month will still fit
                if (remaining_days > 5)
                {
                    gantt_header_top = date.toString("MMM yyyy");
                }
            } else if (date.day() == 1)
            {
                gantt_header_top = date.toString("MMM yyyy");
            }
            date = date.addDays(remaining_days + 1);
            gantt_increment = m_GanttChart_Scale * (remaining_days + 1);
            break;
        }

        case AttributeDisplayFormat_GanttYears:
        {
            const int remaining_days = date.daysInYear() - date.dayOfWeek();
            if (is_start)
            {
                // Check if the name of the month will still fit
                if (remaining_days > 3)
                {
                    gantt_header_top = date.toString("yyyy");
                }
            } else if (date.dayOfWeek() == 1)
            {
                gantt_header_top = date.toString("yyyy");
            }
            date = date.addDays(remaining_days + 1);
            gantt_increment = m_GanttChart_Scale * (remaining_days + 1);
            break;
        }

        default:
        {
            // Error
            const QString reason = tr("Unknown Gantt header format.");
            MessageLogger::Error(CALL_METHOD,
                reason);
            CALL_OUT(reason);
            return;
        }
        }

        if (!gantt_header_top.isEmpty())
        {
            // Render text
            painter.save();
            QTextDocument text;
            const QString html = QString("<b>%1</b>")
                .arg(gantt_header_top);
            text.setHtml(html);
            text.setDocumentMargin(0.0);
            text.setDefaultFont(m_DefaultFont);
            painter.translate(m_AttributePadding + gantt_pos,
                m_RowPadding);
            text.drawContents(&painter);
            painter.restore();
        }
        if (!gantt_header_middle.isEmpty())
        {
            // Render text
            painter.save();
            QTextDocument text;
            const QString html = QString("<b><p align=\"center\">%1</p></b>")
                .arg(gantt_header_middle);
            text.setHtml(html);
            text.setTextWidth(m_GanttChart_Scale);
            text.setDocumentMargin(0.0);
            text.setDefaultFont(m_DefaultFont);
            painter.translate(m_AttributePadding + gantt_pos,
                m_RowPadding + m_GanttChart_HeaderLineHeight);
            text.drawContents(&painter);
            painter.restore();
        }
        if (!gantt_header_bottom.isEmpty())
        {
            // Render text
            painter.save();
            QTextDocument text;
            const QString html = QString("<b><p align=\"center\">%1</p></b>")
                .arg(gantt_header_bottom);
            text.setHtml(html);
            text.setTextWidth(m_GanttChart_Scale);
            text.setDocumentMargin(0.0);
            text.setDefaultFont(m_DefaultFont);
            painter.translate(m_AttributePadding + gantt_pos,
                m_RowPadding + 2*m_GanttChart_HeaderLineHeight);
            text.drawContents(&painter);
            painter.restore();
        }

        // Next section
        is_start = false;
        gantt_pos += gantt_increment;
    }

    // Horizontal line
    painter.drawLine(0,
        m_HeaderHeight - 1,
        m_AttributeWidths[Attribute_GanttChart],
        m_HeaderHeight - 1);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task item: Gantt Chart
void ProjectEditor::CreateRowImage_TaskItem_GanttChart(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(QString::number(mcIndex)));

    // Make sure schedule is up to date
    Project * p = Project::Instance();
    p -> UpdateSchedule();

    // Abbreviations
    const int task_id = m_VisibleIDs[mcIndex];

    // Image
    const int row_height = GetRowImageHeight(mcIndex);
    QImage image(m_AttributeWidths[Attribute_GanttChart],
        row_height,
        QImage::Format_RGB32);
    image.fill(m_BackgroundColors[mcIndex % 2]);
    QPainter painter(&image);
    if (m_SelectedTaskIDs.contains(task_id))
    {
        painter.save();
        painter.setOpacity(m_SelectedOpacity);
        painter.fillRect(0, 0, image.width(), image.height(),
            m_SelectedIndexColor);
        painter.restore();
    }
    if (m_HoveredIDType == AllTaskGroups::ElementType_TaskID &&
        m_HoveredID == task_id)
    {
        painter.save();
        painter.setOpacity(m_HoverOpacity);
        painter.fillRect(0, 0, image.width(), image.height(), Qt::blue);
        painter.restore();
    }

    // "Today" line
    const int today_offset =
        m_GanttChart_StartDate.daysTo(m_GanttChart_CurrentDate);
    const int today_x =
        m_AttributePadding + (today_offset + 0.5) * m_GanttChart_Scale;
    painter.save();
    painter.setPen(m_TodayColor);
    painter.setOpacity(m_TodayOpacity);
    painter.drawLine(
        today_x,
        0,
        today_x,
        row_height);
    painter.restore();

    // Get task information
    AllTaskItems * at = AllTaskItems::Instance();
    QHash < AllTaskItems::Information, QString > information =
        at -> GetInformation(task_id);

    // Gantt chart
    QDate start_date = QDate::fromString(
        information[AllTaskItems::Information_EarlyStart], "yyyy-MM-dd");
    QDate finish_date = QDate::fromString(
        information[AllTaskItems::Information_EarlyFinish], "yyyy-MM-dd");
    const int offset_start = m_GanttChart_StartDate.daysTo(start_date);
    const int offset_finish = m_GanttChart_StartDate.daysTo(finish_date);
    const QColor gantt_color =
        information[AllTaskItems::Information_IsOnCriticalPath] == "yes" ?
            m_GanttCriticalPathColor : m_GanttBarColor;
    if (information[AllTaskItems::Information_IsMilestone] == "yes")
    {
        QPainterPath milestone;
        const int milestone_start = m_GanttChart_BarWestPadding
            + offset_start * m_GanttChart_Scale
            - m_GanttChart_BarMilestoneWidth/2;
        milestone.moveTo(milestone_start,
            m_GanttChart_BarNorthPadding + m_GanttChart_BarHeight);
        milestone.lineTo(milestone_start + m_GanttChart_BarMilestoneWidth,
            m_GanttChart_BarNorthPadding + m_GanttChart_BarHeight);
        milestone.lineTo(milestone_start + m_GanttChart_BarMilestoneWidth/2,
            m_GanttChart_BarNorthPadding);
        milestone.lineTo(milestone_start,
            m_GanttChart_BarNorthPadding + m_GanttChart_BarHeight);
        painter.fillPath(milestone, gantt_color);
    } else
    {
        painter.fillRect(
            m_GanttChart_BarWestPadding + offset_start * m_GanttChart_Scale,
            m_GanttChart_BarNorthPadding,
            (offset_finish - offset_start + 1) * m_GanttChart_Scale,
            m_GanttChart_BarHeight,
            gantt_color);
    }

    // Done
    m_TaskItemIDToImage_GanttChart[task_id] = image;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Task group image: Gantt chart
void ProjectEditor::CreateRowImage_TaskGroup_GanttChart(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(QString::number(mcIndex)));

    // Private method - no checks

    // Make sure schedule is up to date
    Project * p = Project::Instance();
    p -> UpdateSchedule();

    // Abbreviations
    const int group_id = m_VisibleIDs[mcIndex];

    // Initialize image
    const int row_height = GetRowImageHeight(mcIndex);
    QImage image(m_AttributeWidths[Attribute_GanttChart],
        row_height,
        QImage::Format_RGB32);
    image.fill(m_BackgroundColors[mcIndex % 2]);
    QPainter painter(&image);
    if (m_SelectedGroupIDs.contains(group_id))
    {
        painter.save();
        painter.setOpacity(m_SelectedOpacity);
        painter.fillRect(0, 0, image.width(), image.height(),
            m_SelectedIndexColor);
        painter.restore();
    }
    if (m_HoveredIDType == AllTaskGroups::ElementType_GroupID &&
        m_HoveredID == group_id)
    {
        painter.save();
        painter.setOpacity(m_HoverOpacity);
        painter.fillRect(0, 0, image.width(), image.height(), Qt::blue);
        painter.restore();
    }

    // "Today" line
    const int today_offset =
        m_GanttChart_StartDate.daysTo(m_GanttChart_CurrentDate);
    const int today_x =
        m_AttributePadding + (today_offset + 0.5) * m_GanttChart_Scale;
    painter.save();
    painter.setPen(m_TodayColor);
    painter.setOpacity(m_TodayOpacity);
    painter.drawLine(
        today_x,
        0,
        today_x,
        row_height);
    painter.restore();

    // Store image
    m_TaskGroupIDToImage_GanttChart[group_id] = image;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Get image, regardless of type
QImage ProjectEditor::GetRowImage_GanttChart(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(QString::number(mcIndex)));

    // Private method - no checks

    if (m_VisibleIDTypes[mcIndex] ==
        AllTaskGroups::ElementType_TaskID)
    {
        const int task_id = m_VisibleIDs[mcIndex];
        if (!m_TaskItemIDToImage_GanttChart.contains(task_id))
        {
            CreateRowImage_TaskItem_GanttChart(mcIndex);
        }
        CALL_OUT("");
        return m_TaskItemIDToImage_GanttChart[task_id];
    } else if (m_VisibleIDTypes[mcIndex] ==
        AllTaskGroups::ElementType_GroupID)
    {
        const int group_id = m_VisibleIDs[mcIndex];
        if (!m_TaskGroupIDToImage_GanttChart.contains(group_id))
        {
            CreateRowImage_TaskGroup_GanttChart(mcIndex);
        }
        CALL_OUT("");
        return m_TaskGroupIDToImage_GanttChart[group_id];
    } else
    {
        // Error
        const QString reason = tr("Unknown index type.");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return QImage();
    }
}



///////////////////////////////////////////////////////////////////////////////
// Calender (holidays) changed
void ProjectEditor::HolidaysChanged()
{
    CALL_IN("");

    // Redo header and all Gantt chart elements
    m_HeaderImage_GanttChart = QImage();
    m_TaskGroupIDToImage_GanttChart.clear();
    m_TaskItemIDToImage_GanttChart.clear();

    update();

    CALL_OUT("");
}



// =============================================================== GUI: Drawing



///////////////////////////////////////////////////////////////////////////////
// Initialize drawing pieces of information
void ProjectEditor::Initialize_Drawing()
{
    CALL_IN("");

    // Header images
    m_HeaderHeight = 0;

    // Show project schedule from the top
    m_TopIndex = INVALID_INDEX;
    m_TopOffset = 0;
    m_LeftOffset = 0;

    // Attributes shown
    m_VisibleAttributesLeftCoordinates.clear();
    m_VisibleAttributesRightCoordinates.clear();

    // Rows shown
    m_VisibleIDTopCoordinates.clear();
    m_VisibleIDBottomCoordinates.clear();

    // Drag and drop
    m_DragStartPosition = QPoint();
    m_DragAttribute = Attribute_Invalid;
    m_DragAttributeWidth_Attribute = Attribute_Invalid;
    m_DragAttributeWidth_OriginalWidth = -1;

    // Selection
    m_SelectRange_AnchorIndex = INVALID_INDEX;

    // No line is being hovered
    m_HoveredIndex = INVALID_INDEX;
    m_HoveredID = AllTaskGroups::INVALID_ID;
    m_HoveredIDType = AllTaskGroups::ElementType_Invalid;
    m_HoveredAttribute = Attribute_Invalid;
    m_HoveredCell_X = 0;
    m_HoveredCell_Y = 0;
    m_HoveredCellAction = CellAction_Invalid;

    // Cell actions
    m_CellActionTitles[CellAction_Add] = "add";
    m_CellActionTitles[CellAction_Subtract] = "subtract";
    m_CellActionTitles[CellAction_Edit] = "edit";
    m_CellActionTitles[CellAction_NotStarted] = "not started";
    m_CellActionTitles[CellAction_Started] = "started";
    m_CellActionTitles[CellAction_Completed] = "completed";

    // Images we'll need
    m_ImagePlus = QImage(":/resources/Plus.png");
    m_ImageMinus = QImage(":/resources/Minus.png");
    m_ImageEdit = QImage(":/resources/Edit.png");
    m_ImageRed = QImage(":/resources/Red.png");
    m_ImageYellow = QImage(":/resources/Yellow.png");
    m_ImageGreen = QImage(":/resources/Green.png");

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Mouse wheel
void ProjectEditor::wheelEvent(QWheelEvent * mpEvent)
{
    CALL_IN("mpEvent=...");

    // Accept event
    mpEvent -> accept();

    // Check if we're scolling on Gantt chart
    if (!m_GanttChart_StartDateIsLocked &&
        mpEvent -> position().x() + m_LeftOffset > m_AttributesTotalWidth)
    {
        // Yup. We can scroll left/right to change start date
        const int day_delta = mpEvent -> angleDelta().x() / 10;
        const QDate new_start_date =
            m_GanttChart_StartDate.addDays(day_delta);
        SetGanttChartStartDate(new_start_date);
    } else
    {
        // Update left offset
        int new_left_offset =
            m_LeftOffset + mpEvent -> angleDelta().x() / 2;
        new_left_offset = qMax(0, new_left_offset);
        new_left_offset = qMin(GetMaximumLeftOffset(), new_left_offset);
        SetLeftOffset(new_left_offset);
    }

    // Update top offset
    int new_top_offset =
        GetTopOffset() - mpEvent -> angleDelta().y() / 2;
    new_top_offset = qMax(0, new_top_offset);
    new_top_offset = qMin(GetMaximumTopOffset(), new_top_offset);
    SetTopOffset(new_top_offset);

    // Let others know top left coordinates changed
    emit TopLeftChanged();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Redraw
void ProjectEditor::paintEvent(QPaintEvent * mpEvent)
{
    CALL_IN("mpEvent=...");

    // Accept event
    mpEvent -> accept();

    // Check for any necessary updates if current day changed
    CheckIfCurrentDateChanged();

    // Reset show element information
    m_VisibleIDTopCoordinates.clear();
    m_VisibleIDBottomCoordinates.clear();

    // Make sure header exists
    if (m_HeaderImage_Attributes.isNull() ||
        m_HeaderImage_GanttChart.isNull())
    {
        UpdateHeaderHeight();
        UpdateHeaderImage_Attributes();
        UpdateHeaderImage_GanttChart();
    }

    // Header image
    QImage header_image;
    if (m_VisibleAttributes.contains(Attribute_GanttChart))
    {
        header_image = QImage(
            m_AttributesTotalWidth + m_AttributeWidths[Attribute_GanttChart],
            m_HeaderHeight,
            QImage::Format_RGB32);
        QPainter header_painter(&header_image);
        header_painter.drawPixmap(0,
            0,
            m_AttributesTotalWidth,
            m_HeaderHeight,
            QPixmap::fromImage(m_HeaderImage_Attributes),
            0,
            0,
            m_AttributesTotalWidth,
            m_HeaderHeight);
        header_painter.drawPixmap(m_AttributesTotalWidth,
            0,
            m_AttributeWidths[Attribute_GanttChart],
            m_HeaderHeight,
            QPixmap::fromImage(m_HeaderImage_GanttChart),
            0,
            0,
            m_AttributeWidths[Attribute_GanttChart],
            m_HeaderHeight);
    } else
    {
        header_image = m_HeaderImage_Attributes;
    }

    // Paint header
    QPainter painter(this);
    const int visible_width = width();
    painter.drawPixmap(0,
        0,
        visible_width,
        m_HeaderHeight,
        QPixmap::fromImage(header_image),
        m_LeftOffset,
        0,
        visible_width,
        m_HeaderHeight);

    // Determine visible IDs
    if (m_VisibleIDs.isEmpty())
    {
        // Nothing to show.
        CALL_OUT("");
        return;
    }

    // Paint task/group
    const int bottom = height();
    int current_top = m_HeaderHeight;
    for (int index = 0; index < m_VisibleIDs.size(); index++)
    {
        // All indices not shown because they are above the visible area
        if (index < m_TopIndex)
        {
            m_VisibleIDTopCoordinates << -1;
            m_VisibleIDBottomCoordinates << -1;
            continue;
        }

        // All indices not shown because they are below the visible area
        if (current_top > bottom)
        {
            m_VisibleIDTopCoordinates << -1;
            m_VisibleIDBottomCoordinates << -1;
            continue;
        }

        // This is a visible index

        // Render images
        const QImage image_attributes = GetRowImage_Attributes(index);
        const QImage image_ganttchart = GetRowImage_GanttChart(index);
        const int row_height = GetRowImageHeight(index);
        QImage row_image(
            m_AttributesTotalWidth + m_AttributeWidths[Attribute_GanttChart],
            row_height,
            QImage::Format_RGB32);
        QPainter row_painter(&row_image);
        row_painter.drawPixmap(0,
            0,
            m_AttributesTotalWidth,
            row_height,
            QPixmap::fromImage(image_attributes),
            0,
            0,
            m_AttributesTotalWidth,
            row_height);
        int overall_width = m_AttributesTotalWidth;
        if (m_VisibleAttributes.contains(Attribute_GanttChart))
        {
            row_painter.drawPixmap(m_AttributesTotalWidth,
                0,
                m_AttributeWidths[Attribute_GanttChart],
                row_height,
                QPixmap::fromImage(image_ganttchart),
                0,
                0,
                m_AttributeWidths[Attribute_GanttChart],
                row_height);
            overall_width += m_AttributeWidths[Attribute_GanttChart];
        }

        if (index == m_TopIndex)
        {
            painter.drawPixmap(0,
                current_top,
                overall_width - m_LeftOffset,
                m_TopOffset,
                QPixmap::fromImage(row_image),
                m_LeftOffset,
                row_height - m_TopOffset,
                overall_width - m_LeftOffset,
                m_TopOffset);
            m_VisibleIDTopCoordinates << current_top;
            m_VisibleIDBottomCoordinates << current_top + m_TopOffset;
            current_top += m_TopOffset;
        } else
        {
            painter.drawPixmap(0,
                current_top,
                overall_width - m_LeftOffset,
                row_height,
                QPixmap::fromImage(row_image),
                m_LeftOffset,
                0,
                overall_width - m_LeftOffset,
                row_height);
            m_VisibleIDTopCoordinates << current_top;
            m_VisibleIDBottomCoordinates << current_top + row_height;
            current_top += row_height;
        }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Update header height (full)
void ProjectEditor::UpdateHeaderHeight()
{
    CALL_IN("");

    const int height_attributes = CalculateMinimumHeaderHeight_Attributes();
    const int height_ganttchart = CalculateMinimumHeaderHeight_GanttChart();
    const int new_height =
        qMax(height_attributes, height_ganttchart) + 2*m_RowPadding;
    if (m_HeaderHeight != new_height)
    {
        m_HeaderHeight = new_height;
        m_HeaderImage_Attributes = QImage();
        m_HeaderImage_GanttChart = QImage();
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Top offset
int ProjectEditor::GetTopOffset()
{
    CALL_IN("");

    int top = 0;
    for (int index = 0; index < m_TopIndex; index++)
    {
        top += GetRowImageHeight(index);
    }
    if (m_TopIndex != INVALID_INDEX)
    {
        top += GetRowImageHeight(m_TopIndex) - m_TopOffset;
    }

    CALL_OUT("");
    return top;
}



///////////////////////////////////////////////////////////////////////////////
// Left offset
int ProjectEditor::GetLeftOffset() const
{
    CALL_IN("");

    CALL_OUT("");
    return m_LeftOffset;
}



///////////////////////////////////////////////////////////////////////////////
// Maximum top offset
int ProjectEditor::GetMaximumTopOffset()
{
    CALL_IN("");

    int max_top = 0;
    for (int index = 0;
         index < m_VisibleIDs.size();
         index++)
    {
        max_top += GetRowImageHeight(index);
    }
    max_top -= height() - m_HeaderHeight;

    CALL_OUT("");
    return qMax(max_top, 0);
}



///////////////////////////////////////////////////////////////////////////////
// Maximum left offset
int ProjectEditor::GetMaximumLeftOffset() const
{
    CALL_IN("");
    const int max_left =
        m_AttributesTotalWidth + m_AttributeWidths[Attribute_GanttChart]
            - width();

    CALL_OUT("");
    return qMax(max_left, 0);
}



///////////////////////////////////////////////////////////////////////////////
// Scroll to position: top row
void ProjectEditor::SetTopOffset(const int mcNewTopOffset)
{
    CALL_IN(QString("mcNewTopOffset=%1")
        .arg(QString::number(mcNewTopOffset)));

    // Check if new top offset is valid
    if (mcNewTopOffset < 0 ||
        mcNewTopOffset > GetMaximumTopOffset())
    {
        // Error
        const QString reason =
            tr("Invalid value %1 for top offset; valid range is from 0 to %2.")
                .arg(QString::number(mcNewTopOffset),
                     QString::number(GetMaximumTopOffset()));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Set new value
    int top = mcNewTopOffset;
    int index = 0;
    while (index < m_VisibleIDs.size())
    {
        const int row_height = GetRowImageHeight(index);
        if (top < row_height)
        {
            m_TopIndex = index;
            m_TopOffset = GetRowImageHeight(index) - top;
            break;
        }
        top -= GetRowImageHeight(index);
        index++;
    }

    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Scroll to position: left
void ProjectEditor::SetLeftOffset(const int mcNewLeftOffset)
{
    CALL_IN(QString("mcNewLeftOffset=%1")
        .arg(QString::number(mcNewLeftOffset)));

    // Check if new top offset is valid
    if (mcNewLeftOffset < 0 ||
        mcNewLeftOffset > GetMaximumLeftOffset())
    {
        // Error
        const QString reason =
            tr("Invalid value %1 for left offset; "
                "valid range is from 0 to %2.")
                .arg(QString::number(mcNewLeftOffset),
                     QString::number(GetMaximumLeftOffset()));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Set new value
    m_LeftOffset = mcNewLeftOffset;
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Move
void ProjectEditor::mouseMoveEvent(QMouseEvent * mpEvent)
{
    CALL_IN("mpEvent=...");

    // Accept event
    mpEvent -> accept();

    // Abbreviation
    const int x = mpEvent -> pos().x() + m_LeftOffset;
    const int y = mpEvent -> pos().y();

    // Check if mouse is pressed
    if (m_IsLeftMouseButtonPressed)
    {
        // Dragging or about to start to drag
        Drag(x, y);
    } else
    {
        // Hovering
        Hover(x, y);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Clicked
void ProjectEditor::mousePressEvent(QMouseEvent * mpEvent)
{
    CALL_IN("mpEvent=...");

    // Check if left button was pressed
    if (mpEvent -> button() != Qt::LeftButton)
    {
        CALL_OUT("");
        return;
    }

    // Mouse button pressed
    m_IsLeftMouseButtonPressed = true;

    // Where did the user click?
    const int x = mpEvent -> pos().x() + m_LeftOffset;
    const int y = mpEvent -> pos().y();
    if (y < m_HeaderHeight)
    {
        // Clicked on header
        mousePressEvent_Header(x, y);
    } else
    {
        // Clicked on item
        mousePressEvent_Content(x, y);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Single click on the header
void ProjectEditor::mousePressEvent_Header(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // Initialize header drag events
    m_DragAttribute = Attribute_Invalid;
    m_DragAttributeWidth_Attribute = Attribute_Invalid;
    m_DragAttributeWidth_OriginalWidth = 0;

    // Drag start position (widget coordinates)
    m_DragStartPosition = QPoint(mcX, mcY);

    // Check if click was on boundary (to change attribute width)
    // or on attribute itself (to move attribute)
    for (int attr_index = 0;
         attr_index < m_VisibleAttributes.size();
         attr_index++)
    {
        const int attr_left_x =
            m_VisibleAttributesLeftCoordinates[attr_index];
        const int attr_right_x =
            m_VisibleAttributesRightCoordinates[attr_index];

        if (abs(mcX - attr_right_x) < m_SeparatorDragMargin)
        {
            // Dragging attribute boundary
            m_DragAttributeWidth_Attribute = m_VisibleAttributes[attr_index];
            m_DragAttributeWidth_OriginalWidth =
                m_AttributeWidths[m_DragAttributeWidth_Attribute];
            break;
        }

        if (mcX > attr_left_x &&
            mcX < attr_right_x)
        {
            // Dragging attribute
            m_DragAttribute = m_VisibleAttributes[attr_index];
            break;
        }
    }

    // Done
    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Single click on the header
void ProjectEditor::mousePressEvent_Content(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // Get clicked index and attribute
    int clicked_index = GetIndexAtPosition(mcX, mcY);
    Attributes clicked_attribute = GetAttributeAtPosition(mcX, mcY);

    // Check if cell action were activated
    if (clicked_index != INVALID_INDEX &&
        clicked_attribute != Attribute_Invalid &&
        m_HoveredCellAction != CellAction_Invalid)
    {
        // Execute action
        ExecuteCellAction();
        m_IsLeftMouseButtonPressed = false;
        CALL_OUT("");
        return;
    }

    // Check keyboard qualifier
    const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();

    // Thanks to a wonderful "feature" in Qt, Ctrl and Meta are swapped
    // on the Mac.
    const bool command_pressed = (modifiers & Qt::ControlModifier);
    const bool ctrl_pressed = (modifiers & Qt::MetaModifier);
    const bool alt_pressed = (modifiers & Qt::AltModifier);
    const bool shift_pressed = (modifiers & Qt::ShiftModifier);

    // No qualifier: Select just this index or clear selection
    if (!command_pressed &&
        !ctrl_pressed &&
        !alt_pressed &&
        !shift_pressed)
    {
        if (clicked_index == INVALID_INDEX)
        {
            mousePressEvent_Content_DeselectAll();
        } else
        {
            mousePressEvent_Content_SelectSingleItem(clicked_index);
        }
    }

    // Otherwise, an invalid index does nothing to the selection
    if (clicked_index == INVALID_INDEX)
    {
        // No.
        CALL_OUT("");
        return;
    }

    // Shift: Select range
    if (!command_pressed &&
        !ctrl_pressed &&
        !alt_pressed &&
        shift_pressed)
    {
        mousePressEvent_Content_SelectItemRange(clicked_index);
    }

    // Command: Toggle index
    if (command_pressed &&
        !ctrl_pressed &&
        !alt_pressed &&
        !shift_pressed)
    {
        mousePressEvent_Content_ToggleSelectedItem(clicked_index);
    }

    CALL_OUT("");
}




///////////////////////////////////////////////////////////////////////////////
// Update selection: deselect everything
void ProjectEditor::mousePressEvent_Content_DeselectAll()
{
    CALL_IN("");

    SetSelection(QSet < int >(), QSet < int >());

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Update selection: select a single item
void ProjectEditor::mousePressEvent_Content_SelectSingleItem(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(mcIndex));

    // Private -- no checks

    QSet < int > selected_task_ids;
    QSet < int > selected_group_ids;
    if (m_VisibleIDTypes[mcIndex] == AllTaskGroups::ElementType_TaskID)
    {
        selected_task_ids += m_VisibleIDs[mcIndex];
    } else
    {
        selected_group_ids += m_VisibleIDs[mcIndex];
    }
    SetSelection(selected_task_ids, selected_group_ids);

    // Set anchor
    m_SelectRange_AnchorIndex = mcIndex;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Update selection: select a range of items
void ProjectEditor::mousePressEvent_Content_SelectItemRange(const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(mcIndex));

    // Set anchor if necessary
    if (m_SelectRange_AnchorIndex == INVALID_INDEX)
    {
        m_SelectRange_AnchorIndex = mcIndex;
    }

    // Select items
    QSet < int > selected_task_ids;
    QSet < int > selected_group_ids;
    for (int index = qMin(m_SelectRange_AnchorIndex, mcIndex);
        index <= qMax(m_SelectRange_AnchorIndex, mcIndex);
        index++)
    {
        if (m_VisibleIDTypes[index] == AllTaskGroups::ElementType_TaskID)
        {
            selected_task_ids += m_VisibleIDs[index];
        } else
        {
            selected_group_ids += m_VisibleIDs[index];
        }
    }
    SetSelection(selected_task_ids, selected_group_ids);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Update selection: toggle a selected item
void ProjectEditor::mousePressEvent_Content_ToggleSelectedItem(
    const int mcIndex)
{
    CALL_IN(QString("mcIndex=%1")
        .arg(mcIndex));

    // Check item type
    if (m_VisibleIDTypes[mcIndex] == AllTaskGroups::ElementType_TaskID)
    {
        const int task_id = m_VisibleIDs[mcIndex];
        QSet < int > selected_task_ids = m_SelectedTaskIDs;
        if (m_SelectedTaskIDs.contains(task_id))
        {
            selected_task_ids.remove(task_id);
        } else
        {
            selected_task_ids += task_id;
        }
        SetSelectedTaskIDs(selected_task_ids);
    } else
    {
        const int group_id = m_VisibleIDs[mcIndex];
        QSet < int > selected_group_ids = m_SelectedGroupIDs;
        if (m_SelectedGroupIDs.contains(group_id))
        {
            selected_group_ids.remove(group_id);
        } else
        {
            selected_group_ids += group_id;
        }
        SetSelectedGroupIDs(selected_group_ids);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Mouse button released
void ProjectEditor::mouseReleaseEvent(QMouseEvent * mpEvent)
{
    CALL_IN("mpEvent=...");

    mpEvent -> accept();

    // Mouse button no longer pressed
    m_IsLeftMouseButtonPressed = false;

    // Stop all dragging activity
    m_DragAttribute = Attribute_Invalid;
    m_DragAttributeWidth_Attribute = Attribute_Invalid;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Double-clicked
void ProjectEditor::mouseDoubleClickEvent(QMouseEvent * mpEvent)
{
    CALL_IN("mpEvent=...");

    // Check for left, right mouse button
    if (mpEvent -> buttons() != Qt::LeftButton)
    {
        // Not left mouse button. We're not interested.
        CALL_OUT("");
        return;
    }

    // Check if we are on top of a cell action
    if (m_HoveredCellAction != CellAction_Invalid)
    {
        // Ignore double clicks on cell actions
        // (so clicking "+" or "-" quickly is not mistaken for double clicking)
        mousePressEvent(mpEvent);
        CALL_OUT("");
        return;
    }

    // !!! If clicked on attribute title, filters could be applied

    // Accept event
    mpEvent -> accept();


    // What do we have....
    switch (m_HoveredIDType)
    {
    case AllTaskGroups::ElementType_TaskID:
        // Tasks will be edited
        Context_EditTask(m_HoveredID);
        break;

    case AllTaskGroups::ElementType_GroupID:
        // Groups will be expanded/collapsed
        if (m_ExpandedTaskGroups.contains(m_HoveredID))
        {
            Context_CollapseGroup(m_HoveredID);
        } else
        {
            Context_ExpandGroup(m_HoveredID);
        }
        break;

    case AllTaskGroups::ElementType_Invalid:
        // Double-clicked on nothing.
        break;

    default:
        // Error
        const QString reason = tr("Invalid type for hovered ID.");
        CALL_OUT(reason);
        return;
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Hovering
void ProjectEditor::Hover(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    if (mcY < m_HeaderHeight)
    {
        Hover_Header(mcX, mcY);
    } else
    {
        Hover_Content(mcX, mcY);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Hovering over header
void ProjectEditor::Hover_Header(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // Check what we're hovering
    Attributes hovered_attribute = GetAttributeAtPosition(mcX, mcY);
    switch (hovered_attribute)
    {
    case Attribute_GanttChart:
        Hover_Header_GanttChart(mcX, mcY);
        break;

    default:
        // No action taken.
        break;
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Hovering over header: Gantt chart
void ProjectEditor::Hover_Header_GanttChart(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    Q_UNUSED(mcY);

    // Check if days are shown - this is the time when holidays
    // are shown
    const AttributeDisplayFormat gantt_header =
        GetEffectiveGanttChartDisplayFormat();
    if (gantt_header != AttributeDisplayFormat_GanttDays &&
        gantt_header != AttributeDisplayFormat_GanttWeekdays)
    {
        // Nothing to do.
        CALL_OUT("");
        return;
    }

    // Check which day
    const int days =
        (mcX - m_AttributesTotalWidth - m_AttributePadding)
            / m_GanttChart_Scale;
    QDate date = m_GanttChart_StartDate.addDays(days);
    Calendar * c = Calendar::Instance();
    if (c -> IsHoliday(date))
    {
        const QStringList holidays = c -> GetHolidayNames(date);
        const QString message = QString("%1: %2")
            .arg(date.toString("dd MMM yyyy"),
                 holidays.join(", "));
        emit ShowMessage(message, false);
    } else
    {
        emit ShowMessage("", false);
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Hovering over content
void ProjectEditor::Hover_Content(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // Check what's under the mouse pointer
    // !!! In this case, mcX is in the global coordinate system, and mcY is
    // !!! the y coordinate of the visible portion...
    const int new_index = GetIndexAtPosition(mcX, mcY);
    int new_id = AllTaskGroups::INVALID_ID;
    AllTaskGroups::ElementType new_type = AllTaskGroups::ElementType_Invalid;
    if (new_index != INVALID_INDEX)
    {
        // Task or group being hovered
        new_id = m_VisibleIDs[new_index];
        new_type = m_VisibleIDTypes[new_index];
    }

    // Update previously hovered element, if necessary
    bool hovered_item_changed = false;
    if (new_id != m_HoveredID ||
        new_type != m_HoveredIDType)
    {
        // Redo old element (does not matter if it's invalid)
        if (m_HoveredIDType == AllTaskGroups::ElementType_TaskID)
        {
            m_TaskItemIDToImage_Attributes.remove(m_HoveredID);
            m_TaskItemIDToImage_GanttChart.remove(m_HoveredID);
            m_TaskIDToRowImageHeight.remove(m_HoveredID);
        } else
        {
            m_TaskGroupIDToImage_Attributes.remove(m_HoveredID);
            m_TaskGroupIDToImage_GanttChart.remove(m_HoveredID);
            m_GroupIDToRowImageHeight.remove(m_HoveredID);
        }
        hovered_item_changed = true;
    }
    if (new_id != AllTaskGroups::INVALID_ID)
    {
        // Redo new element
        if (new_type == AllTaskGroups::ElementType_TaskID)
        {
            m_TaskItemIDToImage_Attributes.remove(new_id);
            m_TaskItemIDToImage_GanttChart.remove(new_id);
            m_TaskIDToRowImageHeight.remove(new_id);
        } else
        {
            m_TaskGroupIDToImage_Attributes.remove(new_id);
            m_TaskGroupIDToImage_GanttChart.remove(new_id);
            m_GroupIDToRowImageHeight.remove(new_id);
        }
        hovered_item_changed = true;
    }

    // Update item
    if (hovered_item_changed)
    {
        m_HoveredIndex = new_index;
        m_HoveredID = new_id;
        m_HoveredIDType = new_type;
    }

    // Clear cell actions
    m_HoveredCell_ActionXMin.clear();
    m_HoveredCell_ActionXMax.clear();
    m_HoveredCell_ActionYMin.clear();
    m_HoveredCell_ActionYMax.clear();
    m_HoveredCell_ActionType.clear();
    m_HoveredCell_ActionData.clear();

    // Anything happening with a row?
    if (m_HoveredIndex != INVALID_INDEX)
    {
        Hover_Content_Row(mcX, mcY);

        // Update coordinates within the cell
        const int index = m_VisibleAttributes.indexOf(m_HoveredAttribute);
        m_HoveredCell_X = mcX
            - m_VisibleAttributesLeftCoordinates[index] - m_AttributePadding;
        m_HoveredCell_Y = mcY
            - m_VisibleIDTopCoordinates[m_HoveredIndex] - m_RowPadding;

        // Update cell actions
        // (needs to happen here; otherwise clicking on the row will not
        // have actions because clicking may happen before we repaint)
        GetRowImage_Attributes(m_HoveredIndex);

        // Determine hovered cell action
        m_HoveredCellAction = CellAction_Invalid;
        for (int action_index = 0;
             action_index < m_HoveredCell_ActionType.size();
             action_index ++)
        {
            if (m_HoveredCell_X >= m_HoveredCell_ActionXMin[action_index] &&
                m_HoveredCell_X < m_HoveredCell_ActionXMax[action_index] &&
                m_HoveredCell_Y >= m_HoveredCell_ActionYMin[action_index] &&
                m_HoveredCell_Y < m_HoveredCell_ActionYMax[action_index])
            {
                m_HoveredCellAction = m_HoveredCell_ActionType[action_index];
                break;
            }
        }
    } else
    {
        m_HoveredCell_X = 0;
        m_HoveredCell_Y = 0;
        m_HoveredCellAction = CellAction_Invalid;
    }

    // Always update as movement within the cell could change the way
    // things look
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Hovering
void ProjectEditor::Hover_Content_Row(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // Check what we're hovering
    Attributes new_hovered_attribute = GetAttributeAtPosition(mcX, mcY);

    // Update if necessary
    if (new_hovered_attribute != m_HoveredAttribute)
    {
        m_HoveredAttribute = new_hovered_attribute;
        if (m_HoveredIDType == AllTaskGroups::ElementType_TaskID)
        {
            m_TaskItemIDToImage_Attributes.remove(m_HoveredID);
            m_TaskItemIDToImage_GanttChart.remove(m_HoveredID);
            m_TaskIDToRowImageHeight.remove(m_HoveredID);
        } else
        {
            m_TaskGroupIDToImage_Attributes.remove(m_HoveredID);
            m_TaskGroupIDToImage_GanttChart.remove(m_HoveredID);
            m_GroupIDToRowImageHeight.remove(m_HoveredID);
        }
    }

    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action
void ProjectEditor::ExecuteCellAction()
{
    CALL_IN("");

    switch (m_HoveredAttribute)
    {
    case Attribute_ID:
        ExecuteCellAction_ID();
        break;

    case Attribute_Title:
        ExecuteCellAction_Title();
        break;

    case Attribute_Duration:
        ExecuteCellAction_Duration();
        break;

    case Attribute_Predecessors:
        ExecuteCellAction_Predecessors();
        break;

    case Attribute_Successors:
        ExecuteCellAction_Successors();
        break;

    case Attribute_CompletionStatus:
        ExecuteCellAction_CompletionStatus();
        break;

    case Attribute_Resources:
        ExecuteCellAction_Resources();
        break;

    case Attribute_Attachments:
        ExecuteCellAction_Attachments();
        break;

    case Attribute_Comments:
        ExecuteCellAction_Comments();
        break;

    default:
        // Error
        const QString reason = tr("No cell actions defined for attribute %1.")
            .arg(m_AttributeSerializationTitles[m_HoveredAttribute]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action: ID
void ProjectEditor::ExecuteCellAction_ID()
{
    CALL_IN("");

    // We know that it's a task item; groups don't have IDs
    const int task_id = m_HoveredID;
    switch (m_HoveredCellAction)
    {
    case CellAction_Edit:
    {
        AllTaskItems * at = AllTaskItems::Instance();
        QHash < AllTaskItems::Information, QString > task_info =
            at -> GetInformation(task_id);

        QDialog * dialog = new QDialog();
        QVBoxLayout * layout = new QVBoxLayout();
        dialog -> setLayout(layout);

        QHBoxLayout * top_layout = new QHBoxLayout();
        QLabel * l_id = new QLabel(tr("New task reference:"));
        top_layout -> addWidget(l_id);
        QLineEdit * le_reference = new QLineEdit();
        le_reference -> setText(
            task_info[AllTaskItems::Information_Reference]);
        le_reference -> selectAll();
        connect (le_reference, SIGNAL(returnPressed()),
            dialog, SLOT(accept()));
        top_layout -> addWidget(le_reference);
        layout -> addLayout(top_layout);

        QHBoxLayout * bottom_layout = new QHBoxLayout();
        bottom_layout -> addStretch(1);
        QPushButton * pb_ok = new QPushButton(tr("Ok"));
        pb_ok -> setFixedWidth(70);
        connect (pb_ok, SIGNAL(clicked()),
            dialog, SLOT(accept()));
        bottom_layout -> addWidget(pb_ok);
        QPushButton * pb_cancel = new QPushButton(tr("Cancel"));
        pb_cancel -> setFixedWidth(70);
        connect (pb_cancel, SIGNAL(clicked()),
            dialog, SLOT(reject()));
        bottom_layout -> addWidget(pb_cancel);
        layout -> addLayout(bottom_layout);

        dialog -> setFixedWidth(300);

        const int result = dialog -> exec();
        const QString new_reference = le_reference -> text().trimmed();
        delete dialog;
        if (result == QDialog::Rejected ||
            !at -> IsReferenceValid(new_reference, task_id))
        {
            break;
        }
        at -> SetInformation(task_id, AllTaskItems::Information_Reference,
            new_reference);

        // Attribute images for the task itself, predecessors, and successors
        // need to be updated
        QList < int > affected_task_ids;
        AllTaskLinks * al = AllTaskLinks::Instance();
        affected_task_ids << task_id
              << al -> GetPredecessorTaskIDsForTaskID(task_id)
              << al -> GetSuccessorTaskIDsForTaskID(task_id);
        for (int affected_task_id : affected_task_ids)
        {
            m_TaskItemIDToImage_Attributes.remove(affected_task_id);
        }
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Cell action %1 not handled.")
            .arg(m_CellActionTitles[m_HoveredCellAction]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action: title
void ProjectEditor::ExecuteCellAction_Title()
{
    CALL_IN("");

    // Abbreviation
    AllTaskItems * at = AllTaskItems::Instance();
    AllTaskGroups * ag = AllTaskGroups::Instance();

    // Get title
    const bool is_task =
        (m_HoveredIDType == AllTaskGroups::ElementType_TaskID);
    QString title;
    if (is_task)
    {
        const QHash < AllTaskItems::Information, QString > info =
            at -> GetInformation(m_HoveredID);
        title = info[AllTaskItems::Information_Title];
    } else
    {
        const QHash < AllTaskGroups::Information, QString > info =
            ag -> GetInformation(m_HoveredID);
        title = info[AllTaskGroups::Information_Title];
    }

    // We know that it's a task item; groups don't have IDs
    switch (m_HoveredCellAction)
    {
    case CellAction_Edit:
    {
        QDialog * dialog = new QDialog();
        QVBoxLayout * layout = new QVBoxLayout();
        dialog -> setLayout(layout);

        QHBoxLayout * top_layout = new QHBoxLayout();
        QLabel * l_id = new QLabel(tr("New %1 title:")
            .arg(is_task ? tr("task") : tr("group")));
        top_layout -> addWidget(l_id);
        QLineEdit * le_title = new QLineEdit();
        le_title -> setText(title);
        le_title -> selectAll();
        connect (le_title, SIGNAL(returnPressed()),
            dialog, SLOT(accept()));
        top_layout -> addWidget(le_title);
        layout -> addLayout(top_layout);

        QHBoxLayout * bottom_layout = new QHBoxLayout();
        bottom_layout -> addStretch(1);
        QPushButton * pb_ok = new QPushButton(tr("Ok"));
        pb_ok -> setFixedWidth(70);
        connect (pb_ok, SIGNAL(clicked()),
            dialog, SLOT(accept()));
        bottom_layout -> addWidget(pb_ok);
        QPushButton * pb_cancel = new QPushButton(tr("Cancel"));
        pb_cancel -> setFixedWidth(70);
        connect (pb_cancel, SIGNAL(clicked()),
            dialog, SLOT(reject()));
        bottom_layout -> addWidget(pb_cancel);
        layout -> addLayout(bottom_layout);

        dialog -> setFixedWidth(300);

        const int result = dialog -> exec();
        const QString new_title = le_title -> text().trimmed();
        delete dialog;
        if (result == QDialog::Rejected ||
            new_title.isEmpty())
        {
            break;
        }
        if (is_task)
        {
            at -> SetInformation(m_HoveredID,
                AllTaskItems::Information_Title,
                new_title);
            m_TaskItemIDToImage_Attributes.remove(m_HoveredID);
        } else
        {
            ag -> SetInformation(m_HoveredID,
                AllTaskGroups::Information_Title,
                new_title);
            m_TaskGroupIDToImage_Attributes.remove(m_HoveredID);
        }
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Cell action %1 not handled.")
            .arg(m_CellActionTitles[m_HoveredCellAction]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action: duration
void ProjectEditor::ExecuteCellAction_Duration()
{
    CALL_IN("");

    // We know that it's a task item; groups don't have durations
    const int task_id = m_HoveredID;
    switch (m_HoveredCellAction)
    {
    case CellAction_Add:
    {
        AllTaskItems * at = AllTaskItems::Instance();
        QHash < AllTaskItems::Information, QString > task_info =
            at -> GetInformation(task_id);
        int duration =
            task_info[AllTaskItems::Information_DurationValue].toInt();
        duration++;
        at -> SetInformation(task_id,
            AllTaskItems::Information_DurationValue,
            QString::number(duration));
        m_TaskItemIDToImage_Attributes.remove(task_id);
        // Need to update all Gantt chart images as critical path may change
        m_TaskItemIDToImage_GanttChart.clear();
        update();
        break;
    }

    case CellAction_Subtract:
    {
        AllTaskItems * at = AllTaskItems::Instance();
        QHash < AllTaskItems::Information, QString > task_info =
            at -> GetInformation(task_id);
        int duration =
            task_info[AllTaskItems::Information_DurationValue].toInt();
        if (duration == 0)
        {
            // Can't have negative duration
            break;
        }
        duration--;
        at -> SetInformation(task_id,
            AllTaskItems::Information_DurationValue,
            QString::number(duration));
        m_TaskItemIDToImage_Attributes.remove(task_id);
        // Need to update all Gantt chart images as critical path may change
        m_TaskItemIDToImage_GanttChart.clear();
        update();
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Cell action %1 not handled.")
            .arg(m_CellActionTitles[m_HoveredCellAction]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action: predecessors
void ProjectEditor::ExecuteCellAction_Predecessors()
{
    CALL_IN("");

    // Abbreviation
    AllTaskLinks * al = AllTaskLinks::Instance();

    // We know that it's a task item; groups don't have predecessors
    const int task_id = m_HoveredID;
    const int action_index =
        m_HoveredCell_ActionType.indexOf(m_HoveredCellAction);
    switch (m_HoveredCellAction)
    {
    case CellAction_Edit:
    {
        const int link_id = m_HoveredCell_ActionData[action_index];
        QHash < AllTaskLinks::Information, QString > link_info =
            al -> GetInformation(link_id);
        const int old_predecessor_task_id =
            link_info[AllTaskLinks::Information_PredecessorID].toInt();

        // Edit link
        LinkEditor * editor = new LinkEditor(link_info, task_id);
        const bool success = editor -> exec();
        link_info = editor -> GetInformation();
        delete editor;
        if (success)
        {
            // Save information
            const int link_id = al -> Create();
            al -> SetInformation(link_id, link_info);
            const int predecessor_task_id =
                link_info[AllTaskLinks::Information_PredecessorID].toInt();
            const int successor_task_id =
                link_info[AllTaskLinks::Information_SuccessorID].toInt();
            m_TaskItemIDToImage_Attributes.remove(old_predecessor_task_id);
            m_TaskItemIDToImage_Attributes.remove(predecessor_task_id);
            m_TaskItemIDToImage_Attributes.remove(successor_task_id);
            // Need to update all Gantt chart images as critical path may
            // change
            m_TaskItemIDToImage_GanttChart.clear();
        }
        break;
    }

    case CellAction_Add:
    {
        // Add a predecessor to the task
        QHash < AllTaskLinks::Information, QString > link_info =
            al -> GetDefaultInformation();
        link_info[AllTaskLinks::Information_SuccessorID] =
            QString::number(task_id);

        // Edit link
        LinkEditor * editor = new LinkEditor(link_info, task_id);
        const bool success = editor -> exec();
        link_info = editor -> GetInformation();
        delete editor;
        if (success)
        {
            // Save information
            const int link_id = al -> Create();
            al -> SetInformation(link_id, link_info);
            const int predecessor_task_id =
                link_info[AllTaskLinks::Information_PredecessorID].toInt();
            const int successor_task_id =
                link_info[AllTaskLinks::Information_SuccessorID].toInt();
            m_TaskItemIDToImage_Attributes.remove(predecessor_task_id);
            m_TaskItemIDToImage_Attributes.remove(successor_task_id);
        }
        break;
    }

    case CellAction_Subtract:
    {
        const int link_id = m_HoveredCell_ActionData[action_index];
        QHash < AllTaskLinks::Information, QString > link_info =
            al -> GetInformation(link_id);
        al -> Delete(link_id);
        const int predecessor_task_id =
            link_info[AllTaskLinks::Information_PredecessorID].toInt();
        const int successor_task_id =
            link_info[AllTaskLinks::Information_SuccessorID].toInt();
        m_TaskItemIDToImage_Attributes.remove(predecessor_task_id);
        m_TaskItemIDToImage_Attributes.remove(successor_task_id);
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Cell action %1 not handled.")
            .arg(m_CellActionTitles[m_HoveredCellAction]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action: successors
void ProjectEditor::ExecuteCellAction_Successors()
{
    CALL_IN("");

    // Abbreviation
    AllTaskLinks * al = AllTaskLinks::Instance();

    // We know that it's a task item; groups don't have successors
    const int task_id = m_HoveredID;
    const int action_index =
        m_HoveredCell_ActionType.indexOf(m_HoveredCellAction);
    switch (m_HoveredCellAction)
    {
    case CellAction_Edit:
    {
        const int link_id = m_HoveredCell_ActionData[action_index];
        QHash < AllTaskLinks::Information, QString > link_info =
            al -> GetInformation(link_id);
        const int old_successor_task_id =
            link_info[AllTaskLinks::Information_SuccessorID].toInt();

        // Edit link
        LinkEditor * editor = new LinkEditor(link_info, task_id);
        const bool success = editor -> exec();
        link_info = editor -> GetInformation();
        delete editor;
        if (success)
        {
            // Save information
            const int link_id = al -> Create();
            al -> SetInformation(link_id, link_info);
            const int predecessor_task_id =
                link_info[AllTaskLinks::Information_PredecessorID].toInt();
            const int successor_task_id =
                link_info[AllTaskLinks::Information_SuccessorID].toInt();
            m_TaskItemIDToImage_Attributes.remove(old_successor_task_id);
            m_TaskItemIDToImage_Attributes.remove(predecessor_task_id);
            m_TaskItemIDToImage_Attributes.remove(successor_task_id);
            // Need to update all Gantt chart images as critical path may
            // change
            m_TaskItemIDToImage_GanttChart.clear();
        }
        break;
    }

    case CellAction_Add:
    {
        // Add a successor to the task
        QHash < AllTaskLinks::Information, QString > link_info =
            al -> GetDefaultInformation();
        link_info[AllTaskLinks::Information_PredecessorID] =
            QString::number(task_id);

        // Edit link
        LinkEditor * editor = new LinkEditor(link_info, task_id);
        const bool success = editor -> exec();
        link_info = editor -> GetInformation();
        delete editor;
        if (success)
        {
            // Save information
            const int link_id = al -> Create();
            al -> SetInformation(link_id, link_info);
            const int predecessor_task_id =
                link_info[AllTaskLinks::Information_PredecessorID].toInt();
            const int successor_task_id =
                link_info[AllTaskLinks::Information_SuccessorID].toInt();
            m_TaskItemIDToImage_Attributes.remove(predecessor_task_id);
            m_TaskItemIDToImage_Attributes.remove(successor_task_id);
            // Need to update all Gantt chart images as critical path may
            // change
            m_TaskItemIDToImage_GanttChart.clear();
        }
        break;
    }

    case CellAction_Subtract:
    {
        const int link_id = m_HoveredCell_ActionData[action_index];
        QHash < AllTaskLinks::Information, QString > link_info =
            al -> GetInformation(link_id);
        al -> Delete(link_id);
        const int predecessor_task_id =
            link_info[AllTaskLinks::Information_PredecessorID].toInt();
        const int successor_task_id =
            link_info[AllTaskLinks::Information_SuccessorID].toInt();
        m_TaskItemIDToImage_Attributes.remove(predecessor_task_id);
        m_TaskItemIDToImage_Attributes.remove(successor_task_id);
        // Need to update all Gantt chart images as critical path may
        // change
        m_TaskItemIDToImage_GanttChart.clear();
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Cell action %1 not handled.")
            .arg(m_CellActionTitles[m_HoveredCellAction]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action: completion status
void ProjectEditor::ExecuteCellAction_CompletionStatus()
{
    CALL_IN("");

    // Abbreviation
    AllTaskItems * at = AllTaskItems::Instance();

    // We know that it's a task item; groups don't have a completion status
    const int task_id = m_HoveredID;
    switch (m_HoveredCellAction)
    {
    case CellAction_NotStarted:
    {
        at -> SetInformation(task_id,
            AllTaskItems::Information_CompletionStatus,
            "not started");
        m_TaskItemIDToImage_Attributes.remove(task_id);
        m_TaskItemIDToImage_GanttChart.remove(task_id);
        break;
    }

    case CellAction_Started:
    {
        at -> SetInformation(task_id,
            AllTaskItems::Information_CompletionStatus,
            "started");
        m_TaskItemIDToImage_Attributes.remove(task_id);
        m_TaskItemIDToImage_GanttChart.remove(task_id);
        break;
    }

    case CellAction_Completed:
    {
        at -> SetInformation(task_id,
            AllTaskItems::Information_CompletionStatus,
            "completed");
        m_TaskItemIDToImage_Attributes.remove(task_id);
        m_TaskItemIDToImage_GanttChart.remove(task_id);
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Cell action %1 not handled.")
            .arg(m_CellActionTitles[m_HoveredCellAction]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action: resources
void ProjectEditor::ExecuteCellAction_Resources()
{
    CALL_IN("");

    // Abbreviation
    AllTaskItems * at = AllTaskItems::Instance();

    // We know that it's a task item; groups don't have resources
    const int task_id = m_HoveredID;
    const int action_index =
        m_HoveredCell_ActionType.indexOf(m_HoveredCellAction);
    switch (m_HoveredCellAction)
    {
    case CellAction_Add:
    {
        AllResources * ar = AllResources::Instance();
        const QList < int > all_resource_ids = ar -> GetAllIDs();
        QHash < int, QString > all_resources;
        for (int resource_id : all_resource_ids)
        {
            const QHash < AllResources::Information, QString > resource_info =
                ar -> GetInformation(resource_id);
            all_resources[resource_id] =
                resource_info[AllResources::Information_Name];
        }

        QDialog * dialog = new QDialog();
        QVBoxLayout * layout = new QVBoxLayout();
        dialog -> setLayout(layout);

        QHBoxLayout * top_layout = new QHBoxLayout();
        QLabel * l_id = new QLabel(tr("Resource to add:"));
        top_layout -> addWidget(l_id);
        AutocompletionLineEdit * le_resource =
            new AutocompletionLineEdit(all_resources.values(), true);
        connect (le_resource, SIGNAL(ReturnPressed()),
            dialog, SLOT(accept()));
        top_layout -> addWidget(le_resource);
        layout -> addLayout(top_layout);

        QHBoxLayout * bottom_layout = new QHBoxLayout();
        bottom_layout -> addStretch(1);
        QPushButton * pb_ok = new QPushButton(tr("Ok"));
        pb_ok -> setFixedWidth(70);
        connect (pb_ok, SIGNAL(clicked()),
            dialog, SLOT(accept()));
        bottom_layout -> addWidget(pb_ok);
        QPushButton * pb_cancel = new QPushButton(tr("Cancel"));
        pb_cancel -> setFixedWidth(70);
        connect (pb_cancel, SIGNAL(clicked()),
            dialog, SLOT(reject()));
        bottom_layout -> addWidget(pb_cancel);
        layout -> addLayout(bottom_layout);

        dialog -> setFixedWidth(300);

        const int result = dialog -> exec();
        const QString new_resource = le_resource -> GetValue();
        delete dialog;
        if (result == QDialog::Rejected ||
            new_resource.isEmpty())
        {
            break;
        }
        const QList < int > new_resource_ids =
            all_resources.keys(new_resource);
        if (!new_resource_ids.isEmpty())
        {
            // Picked existing resource
            // (there can't be more than one)
            const int new_resource_id = new_resource_ids.first();
            at -> AddResourceID(task_id, new_resource_id);
        } else
        {
            // Entered new resource
            const int new_resource_id = ar -> Create(new_resource);
            at -> AddResourceID(task_id, new_resource_id);
        }
        m_TaskItemIDToImage_Attributes.remove(task_id);
        break;
    }

    case CellAction_Subtract:
    {
        const int resource_id = m_HoveredCell_ActionData[action_index];
        at -> RemoveResourceID(task_id, resource_id);
        m_TaskItemIDToImage_Attributes.remove(task_id);
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Cell action %1 not handled.")
            .arg(m_CellActionTitles[m_HoveredCellAction]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action: attachments
void ProjectEditor::ExecuteCellAction_Attachments()
{
    CALL_IN("");

    // Abbreviation
    AllTaskItems * at = AllTaskItems::Instance();
    AllAttachments * aa = AllAttachments::Instance();

    // We know that it's a task item; groups don't have attachments
    const int task_id = m_HoveredID;
    const int action_index =
        m_HoveredCell_ActionType.indexOf(m_HoveredCellAction);
    switch (m_HoveredCellAction)
    {
    case CellAction_Add:
    {
        const QStringList selected_files = QFileDialog::getOpenFileNames(this,
            tr("Select files to attach"),
            QDir::homePath());
        for (const QString & selected_file : selected_files)
        {
            const int attachment_id = aa -> Create(selected_file);
            if (attachment_id != AllAttachments::INVALID_ID)
            {
                at -> AddAttachment(task_id, attachment_id);
            }
        }
        m_TaskItemIDToImage_Attributes.remove(task_id);
        break;
    }

    case CellAction_Subtract:
    {
        const int attachment_id = m_HoveredCell_ActionData[action_index];
        at -> RemoveAttachment(task_id, attachment_id);
        m_TaskItemIDToImage_Attributes.remove(task_id);
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Cell action %1 not handled.")
            .arg(m_CellActionTitles[m_HoveredCellAction]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Execute cell action: comments
void ProjectEditor::ExecuteCellAction_Comments()
{
    CALL_IN("");

    // Abbreviation
    AllTaskItems * at = AllTaskItems::Instance();
    AllComments * ac = AllComments::Instance();

    // We know that it's a task item; groups don't have comments
    const int task_id = m_HoveredID;
    const int action_index =
        m_HoveredCell_ActionType.indexOf(m_HoveredCellAction);
    switch (m_HoveredCellAction)
    {
    case CellAction_Add:
    {
        QHash < AllComments::Information, QString > comment_info =
            ac -> GetDefaultComment();
        CommentEditor * dialog = new CommentEditor(comment_info);
        const int status = dialog -> exec();
        comment_info = dialog -> GetInformation();
        delete dialog;
        if (status == QDialog::Rejected)
        {
            break;
        }
        const int comment_id = ac -> Create(
            comment_info[AllComments::Information_Title],
            comment_info[AllComments::Information_Text]);
        at -> AddComment(task_id, comment_id);
        m_TaskItemIDToImage_Attributes.remove(task_id);
        break;
    }

    case CellAction_Subtract:
    {
        const int comment_id = m_HoveredCell_ActionData[action_index];
        at -> RemoveComment(task_id, comment_id);
        m_TaskItemIDToImage_Attributes.remove(task_id);
        break;
    }

    case CellAction_Edit:
    {
        const int comment_id = m_HoveredCell_ActionData[action_index];
        QHash < AllComments::Information, QString > comment_info =
            ac -> GetInformation(comment_id);
        CommentEditor * dialog = new CommentEditor(comment_info);
        const int status = dialog -> exec();
        comment_info = dialog -> GetInformation();
        delete dialog;
        if (status == QDialog::Rejected)
        {
            break;
        }
        ac -> SetInformation(comment_id, comment_info);
        m_TaskItemIDToImage_Attributes.remove(task_id);
        break;
    }

    default:
    {
        // Error
        const QString reason = tr("Cell action %1 not handled.")
            .arg(m_CellActionTitles[m_HoveredCellAction]);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Dragging
void ProjectEditor::Drag(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // Check if we are dragging in the header
    if (mcY < m_HeaderHeight)
    {
        Drag_Header(mcX, mcY);
        CALL_OUT("");
        return;
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Dragging: header
void ProjectEditor::Drag_Header(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // Check if we are dragging a separator
    if (m_DragAttributeWidth_Attribute != Attribute_Invalid)
    {
        Drag_Header_Separator(mcX, mcY);
        CALL_OUT("");
        return;
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Dragging header separator
void ProjectEditor::Drag_Header_Separator(const int mcX, const int mcY)
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // Update attribute width
    const int delta = mcX - m_DragStartPosition.x();
    int new_width = m_DragAttributeWidth_OriginalWidth + delta;
    new_width  = qMax(new_width, m_MinAttributeWidth);
    m_AttributeWidths[m_DragAttributeWidth_Attribute] = new_width;
    CalculateAttributesTotalWidth();

    // Everything needs to be redone
    // (Gantt chart needs to be redone because row height may have changed)
    m_HeaderImage_Attributes = QImage();
    m_HeaderImage_GanttChart = QImage();
    m_TaskItemIDToImage_Attributes.clear();
    m_TaskItemIDToImage_GanttChart.clear();
    m_TaskGroupIDToImage_Attributes.clear();
    m_TaskGroupIDToImage_GanttChart.clear();
    m_TaskIDToRowImageHeight.clear();
    m_GroupIDToRowImageHeight.clear();

    // Let everybody know the size changed (in this case the internal
    // size, not the size of the widget)
    emit SizeChanged();

    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Determine which attribute is at a given position
ProjectEditor::Attributes ProjectEditor::GetAttributeAtPosition(const int mcX,
    const int mcY) const
{
    CALL_IN(QString("mcX=%1, mcY=%2")
        .arg(QString::number(mcX),
             QString::number(mcY)));

    // We don't need a y coordidate for this.
    Q_UNUSED(mcY);

    // Check out visible attributes
    for (int index = 0; index < m_VisibleAttributes.size(); index++)
    {
        if (mcX >= m_VisibleAttributesLeftCoordinates[index] &&
            mcX < m_VisibleAttributesRightCoordinates[index])
        {
            CALL_OUT("");
            return m_VisibleAttributes[index];
        }
    }

    // No attribute
    CALL_OUT("");
    return Attribute_Invalid;
}



///////////////////////////////////////////////////////////////////////////////
// Resizing
void ProjectEditor::resizeEvent(QResizeEvent * mpEvent)
{
    CALL_IN(QString("mpEvent=..."));

    mpEvent -> accept();

    // Update offsets if necessary
    const int left_offset = qMin(m_LeftOffset, GetMaximumLeftOffset());
    SetLeftOffset(left_offset);
    const int new_top_offset = qMin(GetTopOffset(), GetMaximumTopOffset());
    SetTopOffset(new_top_offset);

    emit SizeChanged();
    update();

    CALL_OUT("");
}



// =============================================================== Context Menu



///////////////////////////////////////////////////////////////////////////////
// Context menu
void ProjectEditor::contextMenuEvent(QContextMenuEvent * mpEvent)
{
    CALL_IN("mpEvent=...");

    // Accept event
    mpEvent -> accept();

    // Check for header context menu
    if (mpEvent -> pos().y() < m_HeaderHeight)
    {
        ContextMenu_Header(mpEvent -> pos());
    } else
    {
        ContextMenu_Content(mpEvent -> pos());
    }

    CALL_OUT("");
    return;
}



///////////////////////////////////////////////////////////////////////////////
// Context menu for header
void ProjectEditor::ContextMenu_Header(const QPoint mcPosition)
{
    CALL_IN(QString("mcPosition={%1, %2}")
        .arg(QString::number(mcPosition.x()),
             QString::number(mcPosition.y())));

    // Position
    const int x = mcPosition.x() + m_LeftOffset;
    const int y = mcPosition.y();

    // Create menu
    QMenu * menu = new QMenu();
    QAction * action;

    // Find attribute
    const Attributes attribute_hovered = GetAttributeAtPosition(x, y);

    // Show as...
    if (attribute_hovered != Attribute_Invalid &&
        m_AttributeAvailableDisplayFormats[attribute_hovered].size() > 1)
    {
        QMenu * show_as = new QMenu(tr("Show as..."));
        menu -> addMenu(show_as);
        const QList < AttributeDisplayFormat > all_formats =
            m_AttributeAvailableDisplayFormats[attribute_hovered];
        for (const AttributeDisplayFormat format : all_formats)
        {
            if (!m_AttributeDisplayFormatGUITitles.contains(format))
            {
                // Error
                MessageLogger::Error(CALL_METHOD,
                    tr("Unknown display format."));
            } else
            {
                action =
                    new QAction(m_AttributeDisplayFormatGUITitles[format]);
                action -> setCheckable(true);
                action -> setChecked(
                    m_AttributeDisplayFormat[attribute_hovered] == format);
                connect (action, &QAction::triggered,
                    [=](){ContextMenu_SetDisplayFormat(attribute_hovered,
                            format);});
                show_as -> addAction(action);
            }
        }
    }

    menu -> addSeparator();

    // Show/hide attribute
    QList < Attributes > ordered_attributes;
    ordered_attributes << Attribute_ID
        << Attribute_CompletionStatus
        << Attribute_Title
        << Attribute_Duration
        << Attribute_StartDate
        << Attribute_FinishDate
        << Attribute_CriticalPath
        << Attribute_SlackWorkdays
        << Attribute_SlackCalendarDays
        << Attribute_Predecessors
        << Attribute_Successors
        << Attribute_Resources
        << Attribute_Attachments
        << Attribute_Comments
        << Attribute_GanttChart;
    if (ordered_attributes.size() != m_AttributeGUITitles.size())
    {
        // Error
        MessageLogger::Error(CALL_METHOD,
            tr("Ordered attribute list does not cover all available "
                "attributes."));
    }
    for (const Attributes attribute : qAsConst(ordered_attributes))
    {
        action = new QAction(m_AttributeGUITitles[attribute]);
        action -> setCheckable(true);
        action -> setChecked(m_VisibleAttributes.contains(attribute));
        connect (action, &QAction::triggered,
            [=](){ContextMenu_ToggleVisibility(attribute,
                      attribute_hovered);});
        menu -> addAction(action);
    }

    // Show context menu
    const QPoint menu_pos = mapToGlobal(mcPosition);
    menu -> exec(menu_pos);

    // Delete it.
    delete menu;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Header
void ProjectEditor::ContextMenu_ToggleVisibility(
    const ProjectEditor::Attributes mcAttribute,
    const Attributes mcAnchorAttribute)
{
    CALL_IN(QString("mcAttribute=\"%1\", mcAnchorAttribute=\"%2\"")
        .arg(m_AttributeSerializationTitles[mcAttribute],
             m_AttributeSerializationTitles[mcAnchorAttribute]));

    // Private - no checks

    // Check if already visible
    if (m_VisibleAttributes.contains(mcAttribute))
    {
        // Hide attribute
        m_VisibleAttributes.removeAll(mcAttribute);
    } else
    {
        // Gantt Chart is always at the end
        if (mcAttribute == Attribute_GanttChart)
        {
            m_VisibleAttributes << mcAttribute;
        } else
        {
            // Show attribute after anchor
            if (mcAnchorAttribute == Attribute_Invalid)
            {
                // At the end
                m_VisibleAttributes << mcAttribute;
            } else
            {
                // Before anchor
                const int index =
                    m_VisibleAttributes.indexOf(mcAnchorAttribute);
                m_VisibleAttributes.insert(index, mcAttribute);
            }
        }
    }

    // Redo header and items
    if (mcAttribute == Attribute_GanttChart)
    {
        // Header needs to be redone, but Gannt chart for tasks and groups
        // will still be valid.
        m_HeaderImage_GanttChart = QImage();
    } else
    {
        // Attributes need to be redone.
        m_HeaderImage_Attributes = QImage();
        m_TaskItemIDToImage_Attributes.clear();
        m_TaskGroupIDToImage_Attributes.clear();
        CalculateAttributesTotalWidth();
    }

    // Size requirements of the widget changed
    emit SizeChanged();

    // Update visuals
    update();

    // Done
    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Set display format for attribute
void ProjectEditor::ContextMenu_SetDisplayFormat(const Attributes mcAttribute,
    const AttributeDisplayFormat mcNewDisplayFormat)
{
    CALL_IN(QString("mcAttribute=\"%1\", mcNewDisplayFormat=\"%2\"")
        .arg(m_AttributeSerializationTitles[mcAttribute],
             m_AttributeDisplayFormatSerializationTitles[mcNewDisplayFormat]));

    // Check if anything needs to be done in the first place
    if (m_AttributeDisplayFormat[mcAttribute] == mcNewDisplayFormat)
    {
        // Nope.
        CALL_OUT("");
        return;
    }

    // Set new display format
    m_AttributeDisplayFormat[mcAttribute] = mcNewDisplayFormat;

    // Make sure things get refreshed
    if (mcAttribute == Attribute_GanttChart)
    {
        m_HeaderImage_GanttChart = QImage();
        m_TaskItemIDToImage_GanttChart.clear();
        m_TaskGroupIDToImage_GanttChart.clear();
    } else
    {
        // Row heights may change because test may become longer or shorter
        m_TaskItemIDToImage_Attributes.clear();
        m_TaskItemIDToImage_GanttChart.clear();
        m_TaskGroupIDToImage_Attributes.clear();
        m_TaskGroupIDToImage_GanttChart.clear();
        m_TaskIDToRowImageHeight.clear();
        m_GroupIDToRowImageHeight.clear();
    }
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu for content
void ProjectEditor::ContextMenu_Content(const QPoint mcPosition)
{
    CALL_IN(QString("mcPosition={%1, %2}")
        .arg(QString::number(mcPosition.x()),
             QString::number(mcPosition.y())));

    // Dispatch hovered item (if any)
    switch (m_HoveredIDType)
    {
    case AllTaskGroups::ElementType_TaskID:
    {
        // Hovering task
        if (!m_SelectedTaskIDs.contains(m_HoveredID))
        {
            // Right click on a single new task selects only this task
            QSet < int > selected_task_ids;
            selected_task_ids += m_HoveredID;
            SetSelection(selected_task_ids, QSet < int >());
            update();
        }
        break;
    }

    case AllTaskGroups::ElementType_GroupID:
    {
        // Hovering group
        if (!m_SelectedGroupIDs.contains(m_HoveredID))
        {
            // Right click on a single new task group selects only this task
            // group
            QSet < int > selected_group_ids;
            selected_group_ids += m_HoveredID;
            SetSelection(QSet < int >(), selected_group_ids);
            update();
        }
        break;
    }

    case AllTaskGroups::ElementType_Invalid:
        // Right click on the canvas clears selection
        SetSelection(QSet < int >(), QSet < int >());
        update();

        // Context menu
        ContextMenu_Canvas(mcPosition);
        break;

        default:
    {
        // Error
        const QString reason = tr("Unknown element type");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }
    }

    // Deal with selection
    const int selected_items =
        m_SelectedTaskIDs.size() + m_SelectedGroupIDs.size();
    if (selected_items == 1)
    {
        if (m_SelectedTaskIDs.size() == 1)
        {
            ContextMenu_Task(mcPosition);
        } else
        {
            ContextMenu_Group(mcPosition);
        }
    } else if (selected_items > 1)
    {
        ContextMenu_Selection(mcPosition);
    } else
    {
        // No item selected at all - cannot occur
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu for several selected indices
void ProjectEditor::ContextMenu_Selection(const QPoint mcPosition)
{
    CALL_IN(QString("mcPosition={%1, %2}")
        .arg(QString::number(mcPosition.x()),
             QString::number(mcPosition.y())));

    // Abbreviation
    AllTaskItems * at = AllTaskItems::Instance();

    // Create menu
    QMenu * menu = new QMenu();
    QAction * action;

    // == Tasks

    // Change status
    QMenu * menu_status = new QMenu(tr("Change Status"));
    menu -> addMenu(menu_status);

    action = new QAction(tr("Not started"), this);
    connect (action,
        &QAction::triggered,
        at,
        [=](){at -> SetInformation(m_SelectedTaskIDs,
                        AllTaskItems::Information_CompletionStatus,
                        "not started");});
    action -> setEnabled(m_SelectedGroupIDs.isEmpty());
    menu_status -> addAction(action);

    action = new QAction(tr("Started"), this);
    connect (action,
        &QAction::triggered,
        at,
        [=](){at -> SetInformation(m_SelectedTaskIDs,
                 AllTaskItems::Information_CompletionStatus, "started");});
    action -> setEnabled(m_SelectedGroupIDs.isEmpty());
    menu_status -> addAction(action);

    action = new QAction(tr("Completed"), this);
    connect (action,
        &QAction::triggered,
        at,
        [=](){at -> SetInformation(m_SelectedTaskIDs,
                AllTaskItems::Information_CompletionStatus, "completed");});
    action -> setEnabled(m_SelectedGroupIDs.isEmpty());
    menu_status -> addAction(action);

    // Resources
    QMenu * menu_resources = new QMenu(tr("Resources"));
    menu -> addMenu(menu_resources);

    AllResources * ar = AllResources::Instance();
    QHash < int, QString > resource_id_to_name;
    QHash < int, bool > resource_id_to_menu;
    const QList < int > all_resource_ids = ar -> GetAllIDs();
    for (const int resource_id : all_resource_ids)
    {
        const QHash < AllResources::Information, QString > resource_info =
            ar -> GetInformation(resource_id);
        resource_id_to_name[resource_id] =
            resource_info[AllResources::Information_Name];
        resource_id_to_menu[resource_id] =
            (resource_info[AllResources::Information_ShowInContextMenu] ==
                "yes");
    }
    const QList < int > sorted_resource_ids =
        StringHelper::SortHash(resource_id_to_name);

    QHash < int, int > used_resource_ids;
    for (const int task_id : qAsConst(m_SelectedTaskIDs))
    {
        const QList < int > resource_ids = at -> GetResourceIDs(task_id);
        for (const int resource_id : resource_ids)
        {
            used_resource_ids[resource_id]++;
        }
    }

    bool has_menu_resources = false;
    for (const int resource_id : qAsConst(sorted_resource_ids))
    {
        // Exclude resources that are not shown by default
        if (!resource_id_to_menu[resource_id])
        {
            continue;
        }
        action = new QAction(resource_id_to_name[resource_id], this);
        action -> setCheckable(true);
        if (used_resource_ids.contains(resource_id) &&
            used_resource_ids[resource_id] == m_SelectedTaskIDs.size())
        {
            action -> setChecked(true);
        }
        action -> setEnabled(m_SelectedGroupIDs.isEmpty());
        connect (action,
            &QAction::triggered,
            this,
            [=](){Context_ToggleResource(m_SelectedTaskIDs, resource_id);});
        menu_resources -> addAction(action);

        has_menu_resources = true;
    }
    if (has_menu_resources)
    {
        menu_resources -> addSeparator();
    }
    for (const int resource_id : qAsConst(sorted_resource_ids))
    {
        // Exclude resources we have shown above or that are not shown by
        // default
        if (resource_id_to_menu[resource_id] ||
            !used_resource_ids.contains(resource_id))
        {
            continue;
        }
        action = new QAction(resource_id_to_name[resource_id], this);
        action -> setCheckable(true);
        if (used_resource_ids.contains(resource_id) &&
            used_resource_ids[resource_id] == m_SelectedTaskIDs.size())
        {
            action -> setChecked(true);
        }
        action -> setEnabled(m_SelectedGroupIDs.isEmpty());
        connect (action, &QAction::triggered,
            [=](){Context_ToggleResource(m_SelectedTaskIDs, resource_id);});
        menu_resources -> addAction(action);
    }

    menu_resources -> addSeparator();
    action = new QAction(tr("Add resource"), this);
    action -> setEnabled(m_SelectedGroupIDs.isEmpty());
    connect (action, &QAction::triggered,
         [=](){Context_AddResource(m_SelectedTaskIDs);});
    menu_resources -> addAction(action);


    menu -> addSeparator();


    // == Selection

    // Delete selection
    action = new QAction(tr("Delete selection"), this);
    connect (action, &QAction::triggered,
        [=](){Context_DeleteSelection();});
    menu -> addAction(action);

    const QPoint menu_pos = mapToGlobal(mcPosition);
    menu -> exec(menu_pos);

    // Delete it.
    delete menu;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu for tasks
void ProjectEditor::ContextMenu_Task(const QPoint mcPosition)
{
    CALL_IN(QString("mcPosition={%1, %2}")
        .arg(QString::number(mcPosition.x()),
             QString::number(mcPosition.y())));

    // Create menu
    QMenu * menu = new QMenu();
    QAction * action;

    // Abbreviation
    AllTaskItems * at = AllTaskItems::Instance();

    // Get task information
    const int task_id = m_SelectedTaskIDs.values().first();
    const QHash < AllTaskItems::Information, QString > information =
        at -> GetInformation(task_id);

    // Edit task
    action = new QAction(tr("Edit task"), this);
    connect (action, &QAction::triggered,
        [=](){Context_EditTask(task_id);});
    menu -> addAction(action);

    // Go to start date
    action = new QAction(tr("Go to task start date"), this);
    const QDate start_date =
        QDate::fromString(information[AllTaskItems::Information_EarlyStart],
            "yyyy-MM-dd");
    connect (action, &QAction::triggered,
        [=](){SetGanttChartStartDate(start_date);});
    menu -> addAction(action);

    // Change status
    const QString status =
        information[AllTaskItems::Information_CompletionStatus];
    QMenu * menu_status = new QMenu(tr("Change Status"));
    menu -> addMenu(menu_status);

    action = new QAction(tr("Not started"), this);
    action -> setCheckable(true);
    action -> setChecked(status == "not started");
    connect (action, &QAction::triggered,
         [=](){Context_SetCompletionStatus(task_id, "not started", "");});
    menu_status -> addAction(action);

    QMenu * menu_started = new QMenu(tr("Started"));
    if (!information[AllTaskItems::Information_ActualStart].isEmpty())
    {
        menu_started -> setDisabled(true);
    }
    menu_status -> addMenu(menu_started);

    action = new QAction(tr("Today"), this);
    connect (action, &QAction::triggered,
         [=](){Context_SetCompletionStatus(task_id, "started", "today");});
    menu_started -> addAction(action);

    action = new QAction(tr("On time"), this);
    connect (action, &QAction::triggered,
         [=](){Context_SetCompletionStatus(task_id, "started", "on time");});
    menu_started -> addAction(action);

    action = new QAction(tr("Specify date"), this);
    connect (action, &QAction::triggered,
         [=](){Context_SetCompletionStatus(task_id, "started", "specify");});
    menu_started -> addAction(action);

    QMenu * menu_completed = new QMenu(tr("Completed"));
    if (!information[AllTaskItems::Information_ActualFinish].isEmpty())
    {
        menu_completed -> setDisabled(true);
    }
    menu_status -> addMenu(menu_completed);

    action = new QAction(tr("Today"), this);
    connect (action, &QAction::triggered,
         [=](){Context_SetCompletionStatus(task_id, "completed", "today");});
    menu_completed -> addAction(action);

    action = new QAction(tr("On time"), this);
    connect (action, &QAction::triggered,
         [=](){Context_SetCompletionStatus(task_id, "completed", "on time");});
    menu_completed -> addAction(action);

    action = new QAction(tr("Specify date"), this);
    connect (action, &QAction::triggered,
         [=](){Context_SetCompletionStatus(task_id, "completed", "specify");});
    menu_completed -> addAction(action);

    // Resources
    QMenu * menu_resources = new QMenu(tr("Resources"));
    menu -> addMenu(menu_resources);

    AllResources * ar = AllResources::Instance();
    QHash < int, QString > resource_id_to_name;
    QHash < int, bool > resource_id_to_menu;
    const QList < int > all_resource_ids = ar -> GetAllIDs();
    for (const int resource_id : all_resource_ids)
    {
        const QHash < AllResources::Information, QString > resource_info =
            ar -> GetInformation(resource_id);
        resource_id_to_name[resource_id] =
            resource_info[AllResources::Information_Name];
        resource_id_to_menu[resource_id] =
            (resource_info[AllResources::Information_ShowInContextMenu] ==
                "yes");
    }
    const QList < int > sorted_resource_ids =
        StringHelper::SortHash(resource_id_to_name);

    bool has_menu_resources = false;
    const QList < int > used_resource_ids = at -> GetResourceIDs(task_id);
    for (const int resource_id : qAsConst(sorted_resource_ids))
    {
        // Exclude resources that are not shown by default
        if (!resource_id_to_menu[resource_id])
        {
            continue;
        }
        action = new QAction(resource_id_to_name[resource_id], this);
        action -> setCheckable(true);
        action -> setChecked(used_resource_ids.contains(resource_id));
        connect (action, &QAction::triggered,
             [=](){Context_ToggleResource(task_id, resource_id);});
        menu_resources -> addAction(action);

        has_menu_resources = true;
    }
    if (has_menu_resources)
    {
        menu_resources -> addSeparator();
    }
    for (const int resource_id : qAsConst(sorted_resource_ids))
    {
        // Exclude resources we have shown above or that are not shown by
        // default
        if (resource_id_to_menu[resource_id] ||
            !used_resource_ids.contains(resource_id))
        {
            continue;
        }
        action = new QAction(resource_id_to_name[resource_id], this);
        action -> setCheckable(true);
        action -> setChecked(used_resource_ids.contains(resource_id));
        connect (action, &QAction::triggered,
             [=](){Context_ToggleResource(task_id, resource_id);});
        menu_resources -> addAction(action);
    }

    menu_resources -> addSeparator();
    action = new QAction(tr("Add resource"), this);
    connect (action, &QAction::triggered,
         [=](){Context_AddResource(task_id);});
    menu_resources -> addAction(action);

    // Add comment
    action = new QAction(tr("Add comment"), this);
    connect (action, &QAction::triggered,
        [=](){Context_AddComment(task_id);});
    menu -> addAction(action);

    // Attachments
    QMenu * menu_attachments = new QMenu(tr("Attachments"));
    menu -> addMenu(menu_attachments);

    // Existing attachments
    const QList < int > attachment_ids = at -> GetAttachmentIDs(task_id);
    AllAttachments * aa = AllAttachments::Instance();
    for (const int attachment_id : attachment_ids)
    {
        const QHash < AllAttachments::Information, QString > attachment_info =
            aa -> GetInformation(attachment_id);
        action =
            new QAction(attachment_info[AllAttachments::Information_Name]);
        connect (action, &QAction::triggered,
            [=](){Context_ShowAttachment(attachment_id);});
        menu_attachments -> addAction(action);
    }

    menu_attachments -> addSeparator();

    // Add attachment
    action = new QAction(tr("Add attachment"), this);
    connect (action, &QAction::triggered,
        [=](){Context_AddAttachment(task_id);});
    menu_attachments -> addAction(action);

    // Delete
    action = new QAction(tr("Delete task"), this);
    connect (action, &QAction::triggered,
        [=](){Context_DeleteTask(task_id);});
    menu -> addAction(action);

    menu -> addSeparator();

    // Add task
    action = new QAction(tr("Add task after this task"), this);
    connect (action, &QAction::triggered,
        [=](){Context_AddTaskAfterTask(task_id);});
    menu -> addAction(action);

    // Add group
    action = new QAction(tr("Add group after this task"), this);
    connect (action, &QAction::triggered,
        [=](){Context_AddGroupAfterTask(task_id);});
    menu -> addAction(action);

    const QPoint menu_pos = mapToGlobal(mcPosition);
    menu -> exec(menu_pos);

    // Delete it.
    delete menu;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu for groups
void ProjectEditor::ContextMenu_Group(const QPoint mcPosition)
{
    CALL_IN(QString("mcPosition={%1, %2}")
        .arg(QString::number(mcPosition.x()),
             QString::number(mcPosition.y())));

    // Create menu
    QMenu * menu = new QMenu();
    QAction * action;

    // Abbreviation
    const int group_id = m_SelectedGroupIDs.values().first();

    // Edit
    action = new QAction(tr("Edit group"), this);
    connect (action, &QAction::triggered,
        [=](){Context_EditGroup(group_id);});
    menu -> addAction(action);

    // Collapse/expand group
    if (m_ExpandedTaskGroups.contains(group_id))
    {
        action = new QAction(tr("Collapse group"), this);
        connect (action, &QAction::triggered,
            [=](){Context_CollapseGroup(group_id);});
        menu -> addAction(action);
    } else
    {
        action = new QAction(tr("Expand group"), this);
        connect (action, &QAction::triggered,
            [=](){Context_ExpandGroup(group_id);});
        menu -> addAction(action);
    }

    // Delete
    action = new QAction(tr("Delete group"), this);
    connect (action, &QAction::triggered,
        [=](){Context_DeleteGroup(group_id);});
    menu -> addAction(action);

    menu -> addSeparator();

    // Add task
    action = new QAction(tr("Add task to this group"), this);
    connect (action, &QAction::triggered,
        [=](){Context_AddTaskInGroup(group_id);});
    menu -> addAction(action);

    // Add group
    action = new QAction(tr("Add group to this group"), this);
    connect (action, &QAction::triggered,
        [=](){Context_NewGroupInGroup(group_id);});
    menu -> addAction(action);

    const QPoint menu_pos = mapToGlobal(mcPosition);
    menu -> exec(menu_pos);

    // Delete it.
    delete menu;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu when on canvas (background)
void ProjectEditor::ContextMenu_Canvas(const QPoint mcPosition)
{
    CALL_IN(QString("mcPosition={%1, %2}")
        .arg(QString::number(mcPosition.x()),
             QString::number(mcPosition.y())));

    // Create menu
    QMenu * menu = new QMenu();
    QAction * action;

    // Add task
    action = new QAction(tr("Add task"), this);
    connect (action, &QAction::triggered,
        [=](){Context_AddTaskInGroup(AllTaskGroups::ROOT_ID);});
    menu -> addAction(action);

    // Add group
    action = new QAction(tr("Add group"), this);
    connect (action, &QAction::triggered,
        [=](){Context_NewGroupInGroup(AllTaskGroups::ROOT_ID);});
    menu -> addAction(action);

    const QPoint menu_pos = mapToGlobal(mcPosition);
    menu -> exec(menu_pos);

    // Delete it.
    delete menu;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: edit task
void ProjectEditor::Context_EditTask(const int mcTaskID)
{
    CALL_IN(QString("mcTaskID=%1")
        .arg(QString::number(mcTaskID)));

    TaskEditor * edit = new TaskEditor(mcTaskID);
    const int success = edit -> exec();
    delete (edit);
    if (success == QDialog::Accepted)
    {
        // Task itself changes - recreate image
        m_TaskItemIDToImage_Attributes.remove(mcTaskID);
        m_TaskItemIDToImage_GanttChart.remove(mcTaskID);
        m_TaskIDToRowImageHeight.remove(mcTaskID);

        // Tasks that link to and from this task may also have changed -
        // recreate them as well.
        AllTaskLinks * al = AllTaskLinks::Instance();
        QList < int > link_ids = al -> GetIDsForSuccessorTaskID(mcTaskID);
        for (const int link_id : qAsConst(link_ids))
        {
            const QHash < AllTaskLinks::Information, QString > link_info =
                al -> GetInformation(link_id);
            const int task_id =
                link_info[AllTaskLinks::Information_PredecessorID].toInt();
            m_TaskItemIDToImage_Attributes.remove(task_id);
            m_TaskItemIDToImage_GanttChart.remove(task_id);
            m_TaskIDToRowImageHeight.remove(task_id);
        }
        link_ids = al -> GetIDsForPredecessorTaskID(mcTaskID);
        for (const int link_id : qAsConst(link_ids))
        {
            const QHash < AllTaskLinks::Information, QString > link_info =
                al -> GetInformation(link_id);
            const int task_id =
                link_info[AllTaskLinks::Information_SuccessorID].toInt();
            m_TaskItemIDToImage_Attributes.remove(task_id);
            m_TaskItemIDToImage_GanttChart.remove(task_id);
            m_TaskIDToRowImageHeight.remove(task_id);
        }

        // Update schedule
        Project * p = Project::Instance();
        p -> UpdateSchedule();

        // Show all changes
        update();
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Set completion status
void ProjectEditor::Context_SetCompletionStatus(const int mcTaskID,
    const QString mcNewStatus, const QString mcTimeliness)
{
    CALL_IN(QString("mcTaskID=%1, mcNewStatus=\"%2\"")
        .arg(QString::number(mcTaskID),
             mcNewStatus));

    // Internal - no checks

    // Abbreviation
    AllTaskItems * at = AllTaskItems::Instance();

    // Determine effective date
    QString effective_date;
    if (mcTimeliness.isEmpty())
    {
        // No date to be specified
    } else if (mcTimeliness == "today")
    {
        QDate task_today;
        Calendar * ca = Calendar::Instance();
        if (mcNewStatus == "started")
        {
            task_today = ca -> GetClosestStartDateForTaskID(mcTaskID,
                QDate::currentDate());
            if (task_today != QDate::currentDate())
            {
                QMessageBox::information(this,
                    tr("Task start day shifted"),
                    tr("It's not a workday today; start day for the task will "
                       "be on the next workday instead, %2.")
                        .arg(task_today.toString("dd MMM yyyy")));
            }
        } else
        {
            task_today = ca -> GetClosestFinishDateForTaskID(mcTaskID,
                QDate::currentDate());
            if (task_today != QDate::currentDate())
            {
                QMessageBox::information(this,
                    tr("Task finish day shifted"),
                    tr("It's not a workday today; finish day for the task "
                       "will be on the last workday instead, %2.")
                        .arg(task_today.toString("dd MMM yyyy")));
            }
        }
        effective_date = task_today.toString("yyyy-MM-dd");
    } else if (mcTimeliness == "on time")
    {
        const QHash < AllTaskItems::Information, QString >
            task_info = at -> GetInformation(mcTaskID);
        if (mcNewStatus == "started")
        {
            effective_date = task_info[AllTaskItems::Information_EarlyStart];
        } else
        {
            effective_date = task_info[AllTaskItems::Information_EarlyFinish];
        }
    } else if (mcTimeliness == "specify")
    {
        // !!! Pick a date
    } else
    {
        // Error
        const QString reason = tr("Unknown timeliness \"%1\".")
            .arg(mcTimeliness);
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Set information
    at -> SetInformation(mcTaskID, AllTaskItems::Information_CompletionStatus,
        mcNewStatus);
    if (mcNewStatus == "not started")
    {
        at -> SetInformation(mcTaskID, AllTaskItems::Information_ActualStart,
            QString());
        at -> SetInformation(mcTaskID, AllTaskItems::Information_ActualFinish,
            QString());
    } else if (mcNewStatus == "started")
    {
        at -> SetInformation(mcTaskID, AllTaskItems::Information_ActualStart,
            effective_date);
        at -> SetInformation(mcTaskID, AllTaskItems::Information_ActualFinish,
            QString());
    } else if (mcNewStatus == "completed")
    {
        at -> SetInformation(mcTaskID, AllTaskItems::Information_ActualFinish,
            effective_date);
    }
    m_TaskItemIDToImage_Attributes.remove(mcTaskID);
    m_TaskIDToRowImageHeight.remove(mcTaskID);

    // Parent groups may change (groups have a completions status as well
    // that may be affected by this task completions status changing)
    AllTaskGroups * ag = AllTaskGroups::Instance();
    int parent_group_id = ag -> GetParentGroupIDForTaskID(mcTaskID);
    while (parent_group_id != AllTaskGroups::ROOT_ID)
    {
        m_TaskGroupIDToImage_Attributes.remove(parent_group_id);
        m_GroupIDToRowImageHeight.remove(parent_group_id);
        parent_group_id = ag -> GetParentGroupIDForGroupID(parent_group_id);
    }
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Add/remove resource from a task
void ProjectEditor::Context_ToggleResource(const int mcTaskID,
    const int mcResourceID)
{
    CALL_IN(QString("mcTaskID=%1, mcResourceID=%2")
        .arg(QString::number(mcTaskID),
             QString::number(mcResourceID)));

    QSet < int > task_ids;
    task_ids += mcTaskID;
    Context_ToggleResource(task_ids, mcResourceID);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Add/remove resource from tasks
void ProjectEditor::Context_ToggleResource(const QSet < int > mcTaskIDs,
    const int mcResourceID)
{
    QList < QString > all_ids;
    for (const int task_id : mcTaskIDs)
    {
        all_ids << QString::number(task_id);
    }
    std::sort(all_ids.begin(), all_ids.end());
    CALL_IN(QString("mcTaskIDs={%1}, mcResourceID=%2")
        .arg(all_ids.join(", "),
             QString::number(mcResourceID)));

    // Private - no checks

    AllTaskItems * at = AllTaskItems::Instance();
    bool used_everywhere = true;
    for (const int task_id : qAsConst(mcTaskIDs))
    {
        const QList < int > resource_ids = at -> GetResourceIDs(task_id);
        if (!resource_ids.contains(mcResourceID))
        {
            used_everywhere = false;
            break;
        }
    }

    if (used_everywhere)
    {
        // Remove resource
        at -> RemoveResourceID(mcTaskIDs, mcResourceID);
    } else
    {
        // Add resource
        at -> AddResourceID(mcTaskIDs, mcResourceID);
    }

    // Task images need to be recreated
    for (const int task_id : qAsConst(mcTaskIDs))
    {
        // Gantt chart for this task does not change
        m_TaskItemIDToImage_Attributes.remove(task_id);
        m_TaskIDToRowImageHeight.remove(task_id);
    }
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Add a resource (new or existing)
void ProjectEditor::Context_AddResource(const int mcTaskID)
{
    CALL_IN(QString("mcTaskID=%1")
        .arg(QString::number(mcTaskID)));

    QSet < int > task_ids;
    task_ids += mcTaskID;
    Context_AddResource(task_ids);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Add a resource (new or existing)
void ProjectEditor::Context_AddResource(const QSet < int > mcTaskIDs)
{
    QList < QString > all_ids;
    for (const int task_id : mcTaskIDs)
    {
        all_ids << QString::number(task_id);
    }
    std::sort(all_ids.begin(), all_ids.end());
    CALL_IN(QString("mcTaskIDs={%1}")
        .arg(all_ids.join(", ")));

    // Get resource
    const int resource_id = SelectResource();
    if (resource_id != AllResources::INVALID_ID)
    {
        // Add resource
        AllTaskItems * at = AllTaskItems::Instance();
        at -> AddResourceID(mcTaskIDs, resource_id);
    }

    // Task image needs to be recreated
    for (const int task_id : qAsConst(mcTaskIDs))
    {
        // Gantt chart for this task does not change, but its height could
        // change
        m_TaskItemIDToImage_Attributes.remove(task_id);
        m_TaskItemIDToImage_GanttChart.remove(task_id);
        m_TaskIDToRowImageHeight.remove(task_id);
    }
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: add comment to task
void ProjectEditor::Context_AddComment(const int mcTaskID)
{
    CALL_IN(QString("mcTaskID=%1")
        .arg(QString::number(mcTaskID)));

    QDialog * dialog = new QDialog();
    QGridLayout * layout = new QGridLayout();
    dialog -> setLayout(layout);
    int row = 0;

    // Set title
    AllTaskItems * at = AllTaskItems::Instance();
    const QHash < AllTaskItems::Information, QString > task_info =
        at -> GetInformation(mcTaskID);
    dialog -> setWindowTitle(tr("Add comment to (%1) %2")
        .arg(task_info[AllTaskItems::Information_Reference],
            task_info[AllTaskItems::Information_Title]));

    // First row: title
    QLabel * l_title = new QLabel(tr("Title"));
    layout -> addWidget(l_title, row, 0, Qt::AlignTop);
    QLineEdit * title = new QLineEdit();
    layout -> addWidget(title, row, 1);
    row++;

    // Second row: comment
    QLabel * l_comment = new QLabel(tr("Comment"));
    layout -> addWidget(l_comment, row, 0, Qt::AlignTop);
    QTextEdit * comment = new QTextEdit();
    comment -> setAcceptRichText(false);
    layout -> addWidget(comment, row, 1);
    row++;

    // Bottom row: ok and cancel
    QHBoxLayout * bottom_layout = new QHBoxLayout();
    layout -> addLayout(bottom_layout, row, 0, 1, 2);
    bottom_layout -> addStretch(1);
    QPushButton * ok = new QPushButton(tr("Ok"));
    ok -> setFixedWidth(100);
    connect (ok, SIGNAL(clicked()),
        dialog, SLOT(accept()));
    bottom_layout -> addWidget(ok);
    QPushButton * cancel = new QPushButton(tr("Cancel"));
    cancel -> setFixedWidth(100);
    connect (cancel, SIGNAL(clicked()),
        dialog, SLOT(reject()));
    bottom_layout -> addWidget(cancel);

    // Execute
    const int success = dialog -> exec();
    if (success == QDialog::Accepted)
    {
        QString title_text = title -> text().trimmed();
        QString content_text = comment -> toPlainText().trimmed();
        if (!title_text.isEmpty() && !content_text.isEmpty())
        {
            AllComments * ac = AllComments::Instance();
            const int comment_id = ac -> Create(title_text, content_text);
            at -> AddComment(mcTaskID, comment_id);
            // Gantt chart for this task does not change, but its height could
            // change
            m_TaskItemIDToImage_Attributes.remove(mcTaskID);
            m_TaskItemIDToImage_GanttChart.remove(mcTaskID);
            m_TaskIDToRowImageHeight.remove(mcTaskID);
            update();
        }
    }

    // Done
    delete dialog;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: show existing attachment
void ProjectEditor::Context_ShowAttachment(const int mcAttachmentID)
{
    CALL_IN(QString("mcAttachmentID=%1")
        .arg(QString::number(mcAttachmentID)));

    // Get URL to attachment
    AllAttachments * aa = AllAttachments::Instance();
    const QHash < AllAttachments::Information, QString > info =
        aa -> GetInformation(mcAttachmentID);
    QDesktopServices::openUrl(
        "file://" + info[AllAttachments::Information_LocalFilename]);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: add attachment to task
void ProjectEditor::Context_AddAttachment(const int mcTaskID)
{
    CALL_IN(QString("mcTaskID=%1")
        .arg(QString::number(mcTaskID)));

    // Get file
    // !!! We could reduce this to known attachment types
    const QString base_directory = QDir::homePath();
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Select attachment file"),
        base_directory);
    if (filename.isEmpty())
    {
        // Don't save
        CALL_OUT("");
        return;
    }

    // Save as attachment
    AllAttachments * aa = AllAttachments::Instance();
    const int attachment_id = aa -> Create(filename);
    if (attachment_id == AllAttachments::INVALID_ID)
    {
        // Error has already been reported
        CALL_OUT(tr("Error during creation of the attachment"));
        return;
    }
    AllTaskItems * at = AllTaskItems::Instance();
    at -> AddAttachment(mcTaskID, attachment_id);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: delete a task
void ProjectEditor::Context_DeleteTask(const int mcTaskID)
{
    CALL_IN(QString("mcTaskID=%1")
        .arg(QString::number(mcTaskID)));

    QSet < int > task_ids;
    task_ids += mcTaskID;
    Context_DeleteTasks(task_ids);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: delete tasks
void ProjectEditor::Context_DeleteTasks(const QSet < int > mcTaskIDs)
{
    QList < QString > all_ids;
    for (const int task_id : mcTaskIDs)
    {
        all_ids << QString::number(task_id);
    }
    std::sort(all_ids.begin(), all_ids.end());
    CALL_IN(QString("mcTaskIDs={%1}")
        .arg(all_ids.join(", ")));

    // Abbreviations
    AllTaskItems * at = AllTaskItems::Instance();

    // Make sure we want to delete these task(s)
    QString text;
    if (mcTaskIDs.size() == 1)
    {
        const int task_id = mcTaskIDs.values().first();
        const QHash < AllTaskItems::Information, QString > task_info =
            at -> GetInformation(task_id);
        text = tr("Do you really want to delete task (%1) %2?")
            .arg(task_info[AllTaskItems::Information_Reference],
                task_info[AllTaskItems::Information_Title]);
    } else
    {
        text = tr("Do you really want to delete %1 tasks?")
            .arg(mcTaskIDs.size());
    }
    const QMessageBox::StandardButton result =
        QMessageBox::question(this, tr("Delete tasks"), text);
    if (result != QMessageBox::StandardButton::Yes)
    {
        // Don't do anything
        CALL_OUT("");
        return;
    }

    // Delete task IDs
    for (const int task_id : qAsConst(mcTaskIDs))
    {
        at -> Delete(task_id);
    }

    // Update visuals
    UpdateVisibleIDs();
    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: add new task after this one
void ProjectEditor::Context_AddTaskAfterTask(const int mcTaskID)
{
    CALL_IN(QString("mcTaskID=%1")
        .arg(QString::number(mcTaskID)));

    // Get new name
    AllTaskItems * at = AllTaskItems::Instance();
    const QHash < AllTaskItems::Information, QString > task_info =
        at -> GetInformation(mcTaskID);
    const QString dialog_title = tr("Insert new task after (%1) %2")
        .arg(task_info[AllTaskItems::Information_Reference],
            task_info[AllTaskItems::Information_Title]);
    const QString title_text = GetName(dialog_title);
    if (title_text.isEmpty())
    {
        CALL_OUT("");
        return;
    }

    // OK, let's create a new task
    const int task_id = at -> Create(title_text, 1, "wd");
    if (task_id == AllTaskItems::INVALID_ID)
    {
        CALL_OUT(tr("Error creating new task item."));
        return;
    }

    // Insert at proper position
    AllTaskGroups * ag = AllTaskGroups::Instance();
    const int parent_group_id = ag -> GetParentGroupIDForTaskID(mcTaskID);
    ag -> InsertTaskIDAfterTask(task_id, parent_group_id, mcTaskID);

    // De-select anchor index
    m_SelectRange_AnchorIndex = INVALID_INDEX;
    SetSelection(QSet < int >(), QSet < int >());

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: add new group after this one
void ProjectEditor::Context_AddGroupAfterTask(const int mcTaskID)
{
    CALL_IN(QString("mcTaskID=%1")
        .arg(QString::number(mcTaskID)));

    // Get new name
    AllTaskItems * at = AllTaskItems::Instance();
    const QHash < AllTaskItems::Information, QString > task_info =
        at -> GetInformation(mcTaskID);
    const QString dialog_title = tr("Insert new group after (%1) %2")
        .arg(task_info[AllTaskItems::Information_Reference],
            task_info[AllTaskItems::Information_Title]);
    const QString title_text = GetName(dialog_title);
    if (title_text.isEmpty())
    {
        CALL_OUT("");
        return;
    }

    // OK, let's create a new group
    AllTaskGroups * ag = AllTaskGroups::Instance();
    const int group_id = ag -> Create(title_text);
    if (group_id == AllTaskGroups::INVALID_ID)
    {
        CALL_OUT(tr("Error creating new group."));
        return;
    }

    // Insert at proper position
    const int parent_group_id = ag -> GetParentGroupIDForTaskID(mcTaskID);
    ag -> InsertGroupIDAfterTask(group_id, parent_group_id, mcTaskID);

    // See if we need to expand it
    Preferences * p = Preferences::Instance();
    const QString expand = p -> GetValue("GUI:Expand new groups");
    if (expand == "yes")
    {
        ExpandTaskGroup(group_id);
    }

    // De-select anchor index
    m_SelectRange_AnchorIndex = INVALID_INDEX;
    SetSelection(QSet < int >(), QSet < int >());

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: edit group
void ProjectEditor::Context_EditGroup(const int mcGroupID)
{
    CALL_IN(QString("mcGroupID=%1")
        .arg(QString::number(mcGroupID)));

    GroupEditor * editor = new GroupEditor(mcGroupID);
    const int success = editor -> exec();
    if (success == QDialog::Accepted)
    {
        const QHash < AllTaskGroups::Information, QString > information =
            editor -> GetInformation();
        const QString new_title =
            information[AllTaskGroups::Information_Title].trimmed();
        if (!new_title.isEmpty())
        {
            AllTaskGroups * ag = AllTaskGroups::Instance();
            ag -> SetInformation(mcGroupID, information);
            // Gantt chart for this group does not change, but its height may
            m_TaskGroupIDToImage_Attributes.remove(mcGroupID);
            m_TaskGroupIDToImage_GanttChart.remove(mcGroupID);
            m_GroupIDToRowImageHeight.remove(mcGroupID);
            update();
        }
    }
    delete editor;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: expand group
void ProjectEditor::Context_ExpandGroup(const int mcGroupID)
{
    CALL_IN(QString("mcGroupID=%1")
        .arg(QString::number(mcGroupID)));

    ExpandTaskGroup(mcGroupID);

    // Group did change its image (expansion triangle)
    // (Gantt chart for this group does not change)
    m_TaskGroupIDToImage_Attributes.remove(mcGroupID);
    m_GroupIDToRowImageHeight.remove(mcGroupID);

    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: collapse group
void ProjectEditor::Context_CollapseGroup(const int mcGroupID)
{
    CALL_IN(QString("mcGroupID=%1")
        .arg(QString::number(mcGroupID)));

    CollapseTaskGroup(mcGroupID);

    // Group did change its image (expansion triangle)
    // (Gantt chart for this group does not change)
    m_TaskGroupIDToImage_Attributes.remove(mcGroupID);
    m_GroupIDToRowImageHeight.remove(mcGroupID);

    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: delete group
void ProjectEditor::Context_DeleteGroup(const int mcGroupID)
{
    CALL_IN(QString("mcGroupID=%1")
        .arg(QString::number(mcGroupID)));

    // Internal - no checks

    // Ask for confirmation
    AllTaskGroups * ag = AllTaskGroups::Instance();
    QString dialog_text;
    if (!ag -> IsEmpty(mcGroupID))
    {
        dialog_text = tr("Do you really want to delete this group and its "
            "content?");
    } else
    {
        dialog_text = tr("Do you really want to delete this group?");
    }
    const QMessageBox::StandardButton result =
        QMessageBox::question(this, tr("Delete group"), dialog_text);
    if (result != QMessageBox::StandardButton::Yes)
    {
        // Don't do anything
        CALL_OUT("");
        return;
    }

    // Delete item and its sub elements
    AllTaskItems * at = AllTaskItems::Instance();
    QList < int > groups_to_go;
    groups_to_go << mcGroupID;
    QList < int > delete_group_ids;
    while (!groups_to_go.isEmpty())
    {
        const int group_id = groups_to_go.takeFirst();
        delete_group_ids << group_id;
        const QPair < QList < int >, QList < AllTaskGroups::ElementType > >
            sub_elements = ag -> GetElementIDs(group_id);
        for (int index = 0;
             index < sub_elements.first.size();
             index++)
        {
            const int element_id = sub_elements.first[index];
            const AllTaskGroups::ElementType element_type =
                sub_elements.second[index];
            if (element_type == AllTaskGroups::ElementType_GroupID)
            {
                groups_to_go << element_id;
            } else
            {
                at -> Delete(element_id);
            }
        }
    }
    while (!delete_group_ids.isEmpty())
    {
        const int group_id = delete_group_ids.takeLast();
        ag -> Delete(group_id);
    }

    // De-select anchor index
    m_SelectRange_AnchorIndex = INVALID_INDEX;
    SetSelection(QSet < int >(), QSet < int >());

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: add new task in this group
void ProjectEditor::Context_AddTaskInGroup(const int mcParentGroupID)
{
    CALL_IN(QString("mcParentGroupID=%1")
        .arg(QString::number(mcParentGroupID)));

    // Get new name
    AllTaskGroups * ag = AllTaskGroups::Instance();
    const QHash < AllTaskGroups::Information, QString > group_info =
        ag -> GetInformation(mcParentGroupID);
    const QString dialog_title = tr("Add new task to \"%1\"")
        .arg(group_info[AllTaskGroups::Information_Title]);
    const QString title_text = GetName(dialog_title);
    if (title_text.isEmpty())
    {
        CALL_OUT("");
        return;
    }

    // OK, let's create a new task
    AllTaskItems * at = AllTaskItems::Instance();
    const int task_id = at -> Create(title_text, 1, "wd");
    if (task_id == AllTaskItems::INVALID_ID)
    {
        CALL_OUT(tr("Error creating new task item."));
        return;
    }
    ag -> AddTaskID(task_id, mcParentGroupID);

    // De-select anchor index
    m_SelectRange_AnchorIndex = INVALID_INDEX;
    SetSelection(QSet < int >(), QSet < int >());

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Context menu: add new group in this group
void ProjectEditor::Context_NewGroupInGroup(const int mcParentGroupID)
{
    CALL_IN(QString("mcParentGroupID=%1")
        .arg(QString::number(mcParentGroupID)));

    // Get new name
    AllTaskGroups * ag = AllTaskGroups::Instance();
    const QHash < AllTaskGroups::Information, QString > group_info =
        ag -> GetInformation(mcParentGroupID);
    const QString dialog_title = tr("Add new group to \"%1\"")
        .arg(group_info[AllTaskGroups::Information_Title]);
    const QString title_text = GetName(dialog_title);
    if (title_text.isEmpty())
    {
        CALL_OUT("");
        return;
    }

    // OK, let's create a new group
    const int group_id = ag -> Create(title_text);
    if (group_id == AllTaskGroups::INVALID_ID)
    {
        CALL_OUT(tr("Error creating new group item."));
        return;
    }
    ag -> AddGroupID(group_id, mcParentGroupID);

    // De-select anchor index
    m_SelectRange_AnchorIndex = INVALID_INDEX;
    SetSelection(QSet < int >(), QSet < int >());

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Delete selection
void ProjectEditor::Context_DeleteSelection()
{
    CALL_IN("");

    // Ask for confirmation
    const QMessageBox::StandardButton result =
        QMessageBox::question(this,
            tr("Delete selection"),
            tr("Do you really want to delete the selected items?"));
    if (result != QMessageBox::StandardButton::Yes)
    {
        // Don't do anything
        CALL_OUT("");
        return;
    }

    // Delete tasks
    AllTaskItems * at = AllTaskItems::Instance();
    for (const int task_id : m_SelectedTaskIDs)
    {
        at -> Delete(task_id);
    }

    // Delete groups
    AllTaskGroups * ag = AllTaskGroups::Instance();
    QList < int > groups_to_go = m_SelectedGroupIDs.values();
    QList < int > delete_group_ids;
    while (!groups_to_go.isEmpty())
    {
        const int group_id = groups_to_go.takeFirst();
        delete_group_ids << group_id;
        const QPair < QList < int >, QList < AllTaskGroups::ElementType > >
            sub_elements = ag -> GetElementIDs(group_id);
        for (int index = 0;
             index < sub_elements.first.size();
             index++)
        {
            const int element_id = sub_elements.first[index];
            const AllTaskGroups::ElementType element_type =
                sub_elements.second[index];
            if (element_type == AllTaskGroups::ElementType_GroupID)
            {
                groups_to_go << element_id;
            } else
            {
                at -> Delete(element_id);
            }
        }
    }
    while (!delete_group_ids.isEmpty())
    {
        const int group_id = delete_group_ids.takeLast();
        ag -> Delete(group_id);
    }

    // De-select anchor index
    m_SelectRange_AnchorIndex = INVALID_INDEX;
    SetSelection(QSet < int >(), QSet < int >());

    CALL_OUT("");
}



// ====================================================================== Debug



///////////////////////////////////////////////////////////////////////////////
// Dump everything
void ProjectEditor::Dump() const
{
    CALL_IN("");

    // ProjectEditor details
    qDebug().noquote() << "============================ Project Editor";

    qDebug().noquote() << QString("Visible attributes");
    for (const Attributes attribute : qAsConst(m_VisibleAttributes))
    {
        qDebug().noquote() <<
            QString("  Attribute %1 shown as %2 (alignment %3, width %4)")
                .arg(m_AttributeGUITitles[attribute],
                     m_AttributeDisplayFormatGUITitles[
                        m_AttributeDisplayFormat[attribute]],
                     m_AttributeContentAlignment[attribute],
                     QString::number(m_AttributeWidths[attribute]));
    }

    qDebug().noquote() << QString("Expanded groups");
    AllTaskGroups * ag = AllTaskGroups::Instance();
    for (int group_id : qAsConst(m_ExpandedTaskGroups))
    {
        const QHash < AllTaskGroups::Information, QString > group_info =
            ag -> GetInformation(group_id);
        qDebug().noquote() << QString("    Group ID %1 (%2)")
            .arg(QString::number(group_id),
                 group_info[AllTaskGroups::Information_Title]);
    }

    qDebug().noquote() << QString("Visible IDs");
    AllTaskItems * at = AllTaskItems::Instance();
    for (int index = 0;
         index < m_VisibleIDs.size();
         index++)
    {
        const int element_id = m_VisibleIDs[index];
        const AllTaskGroups::ElementType element_type =
            m_VisibleIDTypes[index];
        if (element_type == AllTaskGroups::ElementType_GroupID)
        {
            const QHash < AllTaskGroups::Information, QString > group_info =
                ag -> GetInformation(element_id);
            qDebug().noquote() << QString("    Group %1 (%2), indent %3")
                .arg(QString::number(element_id),
                    group_info[AllTaskGroups::Information_Title],
                    QString::number(m_VisibleIDIndentation[index]));
        } else
        {
            const QHash < AllTaskItems::Information, QString > task_info =
                at -> GetInformation(element_id);
            qDebug().noquote() << QString("    Task %1 (%2), indent %3")
                .arg(QString::number(element_id),
                    task_info[AllTaskItems::Information_Title],
                    QString::number(m_VisibleIDIndentation[index]));
        }
    }

    CALL_OUT("");
}
