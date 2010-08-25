
#include "lights/controlgroupmodel.h"
#include "lights/lightcontroller.h"

#include "lights/triggermode.h"
#include "lights/controlmode.h"

ControlGroupModel::ControlGroupModel(LightController* pController)
        : m_pLightController(pController) {

}

ControlGroupModel::~ControlGroupModel() {

}

int ControlGroupModel::rowCount(const QModelIndex& index) const {
    if (index.isValid()) {
        return 0;
    }

    return m_pLightController->numControlGroups();
}

int ControlGroupModel::columnCount(const QModelIndex& index) const {
    if (index.isValid()) {
        return 0;
    }
    return ControlGroupModel::NUM_COLUMNS;
}

QModelIndex ControlGroupModel::index(int row, int column, const QModelIndex& parent) const {
    if (row < 0 || row >= rowCount(parent)) {
        return QModelIndex();
    }
    if (column < 0 || column >= columnCount(parent)) {
        return QModelIndex();
    }
    return createIndex(row, column);
}

QModelIndex ControlGroupModel::parent(const QModelIndex& index) const {
    return QModelIndex();
}


QVariant ControlGroupModel::headerData(int section, Qt::Orientation orientation, int role) const {

    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case ControlGroupModel::NAME:
                return QString(tr("Name"));
            case ControlGroupModel::TRIGGER_MODE:
                return QString(tr("Trigger Mode"));
            case ControlGroupModel::CONTROL_MODE:
                return QString(tr("Control Mode"));
            case ControlGroupModel::COLOR_GENERATOR:
                return QString(tr("Color Generator"));
            case ControlGroupModel::NUM_LIGHTS:
                return QString(tr("# Lights"));
            default:
                return QString(tr("Unknown"));
        }
    }

    return QVariant();
}

QVariant ControlGroupModel::data(const QModelIndex& index, int role) const {

    ControlGroup* pGroup = m_pLightController->getControlGroup(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ControlGroupModel::NAME:
                return pGroup->getName();
            case ControlGroupModel::TRIGGER_MODE:
                return triggerModeToString(pGroup->getTriggerMode());
            case ControlGroupModel::CONTROL_MODE:
                return controlModeToString(pGroup->getControlMode());
            case ControlGroupModel::NUM_LIGHTS:
                return pGroup->numLights();
            case ControlGroupModel::COLOR_GENERATOR:
            default:
                return QString(tr("Unknown"));
        }
    }

    return QVariant();
}

// ComboBoxDelegate::ComboBoxDelegate(QStringList choices)
//         : m_choices(choices) {

// }

// QWidget* ComboBoxDelegate::createEditor(QWidget *parent,
//                                         const QStyleOptionViewItem &option,
//                                         const QModelIndex &index) const {
//     QComboBox* pCombo = new QComboBox(parent);
//     QStringListModel* choices = new QStringListModel(m_choices);
//     pCombo->setModel(choices);
// }

// void ComboBoxDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
//     QComboBox* pCombo = dynamic_cast<QComboBox*>(editor);
//     pCombo->setCurrentIndex(combo->findText(sqlModel->data(index).toString()));
// }
