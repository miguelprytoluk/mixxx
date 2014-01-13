#include "effects/effectrack.h"

#include "effects/effectchainmanager.h"

EffectRack::EffectRack(EffectChainManager* pEffectChainManager,
                       const unsigned int iRackNumber)
        : m_iRackNumber(iRackNumber),
          m_group(formatGroupString(m_iRackNumber)),
          m_pEffectChainManager(pEffectChainManager),
          m_controlClearRack(ConfigKey(m_group, "clear")) {
    connect(&m_controlClearRack, SIGNAL(valueChanged(double)),
            this, SLOT(slotClearRack(double)));
}

EffectRack::~EffectRack() {
}

void EffectRack::registerGroup(const QString& group) {
    foreach (EffectChainSlotPointer pChainSlot, m_effectChainSlots) {
        pChainSlot->registerGroup(group);
    }
}

void EffectRack::slotClearRack(double v) {
    if (v > 0) {
        foreach (EffectChainSlotPointer pChainSlot, m_effectChainSlots) {
            pChainSlot->clear();
        }
    }
}

int EffectRack::numEffectChainSlots() const {
    return m_effectChainSlots.size();
}

void EffectRack::addEffectChainSlot() {
    EffectChainSlot* pChainSlot =
            new EffectChainSlot(this, m_iRackNumber, m_effectChainSlots.size());

    // TODO(rryan) How many should we make default? They create controls that
    // the GUI may rely on, so the choice is important to communicate to skin
    // designers.
    pChainSlot->addEffectSlot();
    pChainSlot->addEffectSlot();
    pChainSlot->addEffectSlot();
    pChainSlot->addEffectSlot();

    connect(pChainSlot, SIGNAL(nextChain(const unsigned int, EffectChainPointer)),
            this, SLOT(loadNextChain(const unsigned int, EffectChainPointer)));
    connect(pChainSlot, SIGNAL(prevChain(const unsigned int, EffectChainPointer)),
            this, SLOT(loadPrevChain(const unsigned int, EffectChainPointer)));

    // Register all the existing channels with the new EffectChain
    const QSet<QString>& registeredGroups =
            m_pEffectChainManager->registeredGroups();
    foreach (const QString& group, registeredGroups) {
        pChainSlot->registerGroup(group);
    }

    m_effectChainSlots.append(EffectChainSlotPointer(pChainSlot));
}

EffectChainSlotPointer EffectRack::getEffectChainSlot(int i) {
    if (i < 0 || i >= m_effectChainSlots.size()) {
        qDebug() << "WARNING: Invalid index for getEffectChainSlot";
        return EffectChainSlotPointer();
    }
    return m_effectChainSlots[i];
}

void EffectRack::loadNextChain(const unsigned int iChainSlotNumber,
                               EffectChainPointer pLoadedChain) {
    EffectChainPointer pNextChain = pLoadedChain;
    while (true) {
        pNextChain = m_pEffectChainManager->getNextEffectChain(pNextChain);

        if (!pNextChain) {
            break;
        }

        // No next chain available.
        if (pNextChain == pLoadedChain) {
            pNextChain = EffectChainPointer();
            break;
        }

        // Don't load chains that are already loaded.
        if (pNextChain->enabled()) {
            continue;
        }

        break;
    }
    m_effectChainSlots[iChainSlotNumber]->loadEffectChain(pNextChain);
}


void EffectRack::loadPrevChain(const unsigned int iChainSlotNumber,
                               EffectChainPointer pLoadedChain) {
    EffectChainPointer pPrevChain = pLoadedChain;
    while (true) {
        pPrevChain = m_pEffectChainManager->getPrevEffectChain(pPrevChain);

        if (!pPrevChain) {
            break;
        }

        // No prev chain available.
        if (pPrevChain == pLoadedChain) {
            pPrevChain = EffectChainPointer();
            break;
        }

        // Don't load chains that are already loaded.
        if (pPrevChain->enabled()) {
            continue;
        }

        break;
    }
    m_effectChainSlots[iChainSlotNumber]->loadEffectChain(pPrevChain);
}