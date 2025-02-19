// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scroll/ProgrammaticScrollAnimator.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntPoint.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorAnimation.h"
#include "public/platform/WebCompositorAnimationPlayer.h"
#include "public/platform/WebCompositorAnimationTimeline.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebScrollOffsetAnimationCurve.h"

namespace blink {

PassOwnPtr<ProgrammaticScrollAnimator> ProgrammaticScrollAnimator::create(ScrollableArea* scrollableArea)
{
    return adoptPtr(new ProgrammaticScrollAnimator(scrollableArea));
}

ProgrammaticScrollAnimator::ProgrammaticScrollAnimator(ScrollableArea* scrollableArea)
    : m_compositorAnimationAttachedToLayerId(0)
    , m_scrollableArea(scrollableArea)
    , m_startTime(0.0)
    , m_runState(RunState::Idle)
    , m_compositorAnimationId(0)
    , m_compositorAnimationGroupId(0)
{
    if (RuntimeEnabledFeatures::compositorAnimationTimelinesEnabled()) {
        ASSERT(Platform::current()->compositorSupport());
        m_compositorPlayer = adoptPtr(Platform::current()->compositorSupport()->createAnimationPlayer());
        ASSERT(m_compositorPlayer);
        m_compositorPlayer->setAnimationDelegate(this);
    }
}

ProgrammaticScrollAnimator::~ProgrammaticScrollAnimator()
{
    if (m_compositorPlayer) {
        m_compositorPlayer->setAnimationDelegate(nullptr);
        m_compositorPlayer.clear();
    }
}

void ProgrammaticScrollAnimator::resetAnimationState()
{
    m_animationCurve.clear();
    m_startTime = 0.0;
    m_runState = RunState::Idle;
    m_compositorAnimationId = 0;
    m_compositorAnimationGroupId = 0;
}

void ProgrammaticScrollAnimator::notifyPositionChanged(const DoublePoint& offset)
{
    m_scrollableArea->scrollPositionChanged(offset, ProgrammaticScroll);
}

void ProgrammaticScrollAnimator::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    cancelAnimation();
    notifyPositionChanged(offset);
}

void ProgrammaticScrollAnimator::animateToOffset(FloatPoint offset)
{
    m_startTime = 0.0;
    m_targetOffset = offset;
    m_animationCurve = adoptPtr(Platform::current()->compositorSupport()->createScrollOffsetAnimationCurve(m_targetOffset, WebCompositorAnimationCurve::TimingFunctionTypeEaseInOut));

    m_animationCurve->setInitialValue(FloatPoint(m_scrollableArea->scrollPosition()));
    m_scrollableArea->registerForAnimation();
    if (!m_scrollableArea->scheduleAnimation()) {
        resetAnimationState();
        notifyPositionChanged(IntPoint(offset.x(), offset.y()));
    }
    m_runState = RunState::WaitingToSendToCompositor;
}

void ProgrammaticScrollAnimator::cancelAnimation()
{
    switch (m_runState) {
    case RunState::Idle:
    case RunState::WaitingToCancelOnCompositor:
        break;
    case RunState::WaitingToSendToCompositor:
        if (m_compositorAnimationId) {
            // We still have a previous animation running on the compositor.
            m_runState = RunState::WaitingToCancelOnCompositor;
        } else {
            resetAnimationState();
        }
        break;
    case RunState::RunningOnMainThread:
        resetAnimationState();
        break;
    case RunState::RunningOnCompositor:
        m_runState = RunState::WaitingToCancelOnCompositor;

        // Get serviced the next time compositor updates are allowed.
        m_scrollableArea->registerForAnimation();
    }
}

void ProgrammaticScrollAnimator::tickAnimation(double monotonicTime)
{
    if (m_runState != RunState::RunningOnMainThread)
        return;

    if (!m_startTime)
        m_startTime = monotonicTime;
    double elapsedTime = monotonicTime - m_startTime;
    bool isFinished = (elapsedTime > m_animationCurve->duration());
    FloatPoint offset = m_animationCurve->getValue(elapsedTime);
    notifyPositionChanged(IntPoint(offset.x(), offset.y()));

    if (isFinished) {
        resetAnimationState();
    } else if (!m_scrollableArea->scheduleAnimation()) {
        notifyPositionChanged(IntPoint(m_targetOffset.x(), m_targetOffset.y()));
        resetAnimationState();
    }
}

bool ProgrammaticScrollAnimator::hasAnimationThatRequiresService() const
{
    switch (m_runState) {
    case RunState::Idle:
    case RunState::RunningOnCompositor:
        return false;
    case RunState::WaitingToSendToCompositor:
    case RunState::RunningOnMainThread:
    case RunState::WaitingToCancelOnCompositor:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void ProgrammaticScrollAnimator::updateCompositorAnimations()
{
    if (m_compositorAnimationId && m_runState != RunState::RunningOnCompositor) {
        // If the current run state is WaitingToSendToCompositor but we have a
        // non-zero compositor animation id, there's a currently running
        // compositor animation that needs to be removed here before the new
        // animation is added below.
        ASSERT(m_runState == RunState::WaitingToCancelOnCompositor || m_runState == RunState::WaitingToSendToCompositor);

        if (m_compositorPlayer) {
            if (m_compositorPlayer->isLayerAttached())
                m_compositorPlayer->removeAnimation(m_compositorAnimationId);
        } else {
            if (GraphicsLayer* layer = m_scrollableArea->layerForScrolling())
                layer->removeAnimation(m_compositorAnimationId);
        }

        m_compositorAnimationId = 0;
        m_compositorAnimationGroupId = 0;
        if (m_runState == RunState::WaitingToCancelOnCompositor) {
            resetAnimationState();
            return;
        }
    }

    if (m_runState == RunState::WaitingToSendToCompositor) {
        bool sentToCompositor = false;

        if (GraphicsLayer* layer = m_scrollableArea->layerForScrolling()) {
            if (!layer->platformLayer()->shouldScrollOnMainThread()) {
                OwnPtr<WebCompositorAnimation> animation = adoptPtr(Platform::current()->compositorSupport()->createAnimation(*m_animationCurve, WebCompositorAnimation::TargetPropertyScrollOffset));

                int animationId = animation->id();
                int animationGroupId = animation->group();

                bool animationAdded = false;
                if (m_compositorPlayer) {
                    if (m_compositorPlayer->isLayerAttached()) {
                        m_compositorPlayer->addAnimation(animation.leakPtr());
                        animationAdded = true;
                    }
                } else {
                    animationAdded = m_scrollableArea->layerForScrolling()->addAnimation(animation.release());
                }

                if (animationAdded) {
                    sentToCompositor = true;
                    m_runState = RunState::RunningOnCompositor;
                    m_compositorAnimationId = animationId;
                    m_compositorAnimationGroupId = animationGroupId;
                }
            }
        }

        if (!sentToCompositor) {
            m_runState = RunState::RunningOnMainThread;
            if (!m_scrollableArea->scheduleAnimation()) {
                notifyPositionChanged(IntPoint(m_targetOffset.x(), m_targetOffset.y()));
                resetAnimationState();
            }
        }
    }
}

void ProgrammaticScrollAnimator::reattachCompositorPlayerIfNeeded(WebCompositorAnimationTimeline* timeline)
{
    int compositorAnimationAttachedToLayerId = 0;
    if (m_scrollableArea->layerForScrolling())
        compositorAnimationAttachedToLayerId = m_scrollableArea->layerForScrolling()->platformLayer()->id();

    if (compositorAnimationAttachedToLayerId != m_compositorAnimationAttachedToLayerId) {
        if (m_compositorPlayer && timeline) {
            // Detach from old layer (if any).
            if (m_compositorAnimationAttachedToLayerId) {
                ASSERT(m_compositorPlayer->isLayerAttached());
                m_compositorPlayer->detachLayer();
                timeline->playerDestroyed(*this);
            }
            // Attach to new layer (if any).
            if (compositorAnimationAttachedToLayerId) {
                ASSERT(m_scrollableArea->layerForScrolling());
                timeline->playerAttached(*this);
                m_compositorPlayer->attachLayer(m_scrollableArea->layerForScrolling()->platformLayer());
            }
            m_compositorAnimationAttachedToLayerId = compositorAnimationAttachedToLayerId;
        }
    }
}

void ProgrammaticScrollAnimator::layerForCompositedScrollingDidChange(WebCompositorAnimationTimeline* timeline)
{
    reattachCompositorPlayerIfNeeded(timeline);

    // If the composited scrolling layer is lost during a composited animation,
    // continue the animation on the main thread.
    if (m_runState == RunState::RunningOnCompositor && !m_scrollableArea->layerForScrolling()) {
        m_runState = RunState::RunningOnMainThread;
        m_compositorAnimationId = 0;
        m_compositorAnimationGroupId = 0;
        m_animationCurve->setInitialValue(FloatPoint(m_scrollableArea->scrollPosition()));
        m_scrollableArea->registerForAnimation();
        if (!m_scrollableArea->scheduleAnimation()) {
            resetAnimationState();
            notifyPositionChanged(IntPoint(m_targetOffset.x(), m_targetOffset.y()));
        }
    }
}

void ProgrammaticScrollAnimator::notifyCompositorAnimationFinished(int groupId)
{
    if (m_compositorAnimationGroupId != groupId)
        return;

    m_compositorAnimationId = 0;
    m_compositorAnimationGroupId = 0;

    switch (m_runState) {
    case RunState::Idle:
    case RunState::RunningOnMainThread:
        ASSERT_NOT_REACHED();
        break;
    case RunState::WaitingToSendToCompositor:
        break;
    case RunState::RunningOnCompositor:
    case RunState::WaitingToCancelOnCompositor:
        resetAnimationState();
    }
}

void ProgrammaticScrollAnimator::notifyAnimationStarted(double monotonicTime, int group)
{
}

void ProgrammaticScrollAnimator::notifyAnimationFinished(double monotonicTime, int group)
{
    notifyCompositorAnimationFinished(group);
}

WebCompositorAnimationPlayer* ProgrammaticScrollAnimator::compositorPlayer() const
{
    return m_compositorPlayer.get();
}

} // namespace blink
