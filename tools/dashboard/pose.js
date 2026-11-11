"use strict";
// Visual odometry only. Keep in step with subsystem2/motion_config.h.
// These provisional dimensions must not be used to command the vehicle.
const CONFIG = Object.freeze({countsRev: 2774, circumferenceMm: 204.2,
    trackMm: 125, staleMs: 2000, maxSampleGapMs: 1000, maxCountsPerSec: 10000});
const signedDelta = (current, previous) => (current - previous) | 0;
const newer = (current, previous) => {
    const step = (current - previous) >>> 0;
    return step > 0 && step < 0x80000000;
};
const finite = value => typeof value === 'number' && Number.isFinite(value);
class PoseTracker {
    constructor() {
        this.packet = null;
        this.baseline = null;
        this.pose = {left: 0, right: 0, yaw: 0, pitch: 0, roll: 0};
        this.speed = {left: null, right: null};
        this.motionValid = false;
        this.attitudeValid = false;
        this.continuous = true;
        this.segment = 0;
    }
    accept(data, now = Date.now()) {
        if (!data || data.version !== 1 || !finite(data.receivedAt) ||
            !Number.isInteger(data.bootId) || !Number.isInteger(data.sequence) ||
            (data.topic && !data.topic.endsWith('/telemetry'))) return false;
        const old = this.packet;
        if (old && old.bootId === data.bootId && !newer(data.sequence, old.sequence)) return false;
        const reset = !old || old.bootId !== data.bootId || old.motionEpoch !== data.motionEpoch;
        if (reset) {
            this.pose = {left: 0, right: 0, yaw: 0, pitch: 0, roll: 0};
            this.baseline = null;
            this.continuous = true;
            this.segment++;
        }
        this.packet = data;
        const fresh = now - data.receivedAt <= CONFIG.staleMs && now >= data.receivedAt - 1000;
        this.attitudeValid = fresh && !!(data.valid & 8) && finite(data.pitch) && finite(data.roll);
        if (this.attitudeValid) {
            this.pose.pitch = data.pitch * Math.PI / 180; // S4: positive nose up.
            // S4 side axis is left; model +Z raises the right side.
            this.pose.roll = -data.roll * Math.PI / 180;
        }
        this.motionValid = fresh && !!(data.valid & 1) &&
            Number.isInteger(data.leftCount) && Number.isInteger(data.rightCount) &&
            Number.isInteger(data.motionMs) && Number.isInteger(data.motionSequence) &&
            ((data.publishedMs - data.motionMs) >>> 0) <= 100;
        this.speed = {left: null, right: null};
        if (!this.motionValid) {
            if (this.baseline) this.continuous = false;
            this.baseline = null;
            return true;
        }
        const previous = this.baseline;
        this.baseline = data;
        if (!previous) return true;
        const dt = (data.motionMs - previous.motionMs) >>> 0;
        if (!newer(data.motionSequence, previous.motionSequence) || dt === 0) {
            this.baseline = previous;
            return true;
        }
        const dl = -signedDelta(data.leftCount, previous.leftCount) || 0;
        const dr = signedDelta(data.rightCount, previous.rightCount);
        const limit = CONFIG.maxCountsPerSec * dt / 1000;
        if (dt > CONFIG.maxSampleGapMs || data.receivedAt - previous.receivedAt > CONFIG.staleMs ||
            Math.abs(dl) > limit || Math.abs(dr) > limit) {
            this.continuous = false;
            return true;
        }
        this.speed = {left: dl * 1000 / dt, right: dr * 1000 / dt};
        this.pose.left -= dl / CONFIG.countsRev * Math.PI * 2;
        this.pose.right -= dr / CONFIG.countsRev * Math.PI * 2;
        this.pose.yaw += (dr - dl) * CONFIG.circumferenceMm / CONFIG.countsRev / CONFIG.trackMm;
        return true;
    }
    snapshot(now = Date.now()) {
        const age = this.packet ? Math.max(0, now - this.packet.receivedAt) : Infinity;
        const fresh = !!this.packet && age <= CONFIG.staleMs && now >= this.packet.receivedAt - 1000;
        return {packet: this.packet, age, fresh, segment: this.segment,
            pose: {...this.pose}, speed: fresh ? {...this.speed} : {left: null, right: null},
            motionValid: fresh && this.motionValid, attitudeValid: fresh && this.attitudeValid,
            continuous: this.continuous};
    }
}
module.exports = {PoseTracker, CONFIG, signedDelta};
