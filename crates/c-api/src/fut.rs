use std::{
    ptr,
    task::{RawWaker, RawWakerVTable, Waker},
};

unsafe fn noop_clone(_data: *const ()) -> RawWaker {
    noop_raw_waker()
}

unsafe fn noop(_data: *const ()) {}

const NOOP_WAKER_VTABLE: RawWakerVTable = RawWakerVTable::new(noop_clone, noop, noop, noop);

const fn noop_raw_waker() -> RawWaker {
    RawWaker::new(ptr::null(), &NOOP_WAKER_VTABLE)
}

pub(crate) fn noop_waker() -> Waker {
    unsafe { Waker::from_raw(noop_raw_waker()) }
}

#[inline]
pub fn noop_waker_ref() -> &'static Waker {
    struct SyncRawWaker(RawWaker);
    unsafe impl Sync for SyncRawWaker {}
    static NOOP_WAKER_INSTANCE: SyncRawWaker = SyncRawWaker(noop_raw_waker());
    unsafe { &*(&NOOP_WAKER_INSTANCE.0 as *const RawWaker as *const Waker) }
}


