// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_handle.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_io_thread.h"
#include "device/test/test_device_client.h"
#include "device/test/usb_test_gadget.h"
#include "device/usb/usb_device.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class UsbDeviceHandleTest : public ::testing::Test {
 public:
  UsbDeviceHandleTest()
      : io_thread_(base::TestIOThread::kAutoStart),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 protected:
  base::TestIOThread io_thread_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestDeviceClient device_client_;
};

class TestOpenCallback {
 public:
  TestOpenCallback() {}

  scoped_refptr<UsbDeviceHandle> WaitForResult() {
    run_loop_.Run();
    return device_handle_;
  }

  UsbDevice::OpenCallback GetCallback() {
    return base::Bind(&TestOpenCallback::SetResult, base::Unretained(this));
  }

 private:
  void SetResult(scoped_refptr<UsbDeviceHandle> device_handle) {
    device_handle_ = device_handle;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  scoped_refptr<UsbDeviceHandle> device_handle_;
};

class TestResultCallback {
 public:
  TestResultCallback() {}

  bool WaitForResult() {
    run_loop_.Run();
    return success_;
  }

  UsbDeviceHandle::ResultCallback GetCallback() {
    return base::Bind(&TestResultCallback::SetResult, base::Unretained(this));
  }

 private:
  void SetResult(bool success) {
    success_ = success;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool success_;
};

class TestCompletionCallback {
 public:
  TestCompletionCallback() {}

  void WaitForResult() { run_loop_.Run(); }

  UsbDeviceHandle::TransferCallback GetCallback() {
    return base::Bind(&TestCompletionCallback::SetResult,
                      base::Unretained(this));
  }
  UsbTransferStatus status() const { return status_; }
  size_t transferred() const { return transferred_; }

 private:
  void SetResult(UsbTransferStatus status,
                 scoped_refptr<net::IOBuffer> buffer,
                 size_t transferred) {
    status_ = status;
    transferred_ = transferred;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  UsbTransferStatus status_;
  size_t transferred_;
};

void ExpectTimeoutAndClose(scoped_refptr<UsbDeviceHandle> handle,
                           const base::Closure& quit_closure,
                           UsbTransferStatus status,
                           scoped_refptr<net::IOBuffer> buffer,
                           size_t transferred) {
  EXPECT_EQ(UsbTransferStatus::TIMEOUT, status);
  handle->Close();
  quit_closure.Run();
}

TEST_F(UsbDeviceHandleTest, InterruptTransfer) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(0, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  const UsbInterfaceDescriptor* interface =
      handle->FindInterfaceByEndpoint(0x81);
  EXPECT_TRUE(interface);
  EXPECT_EQ(0, interface->interface_number);
  interface = handle->FindInterfaceByEndpoint(0x01);
  EXPECT_TRUE(interface);
  EXPECT_EQ(0, interface->interface_number);
  EXPECT_FALSE(handle->FindInterfaceByEndpoint(0x82));
  EXPECT_FALSE(handle->FindInterfaceByEndpoint(0x02));

  scoped_refptr<net::IOBufferWithSize> in_buffer(new net::IOBufferWithSize(64));
  TestCompletionCallback in_completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x81, in_buffer.get(),
                          in_buffer->size(),
                          5000,  // 5 second timeout
                          in_completion.GetCallback());

  scoped_refptr<net::IOBufferWithSize> out_buffer(
      new net::IOBufferWithSize(in_buffer->size()));
  TestCompletionCallback out_completion;
  for (int i = 0; i < out_buffer->size(); ++i) {
    out_buffer->data()[i] = i;
  }

  handle->GenericTransfer(UsbTransferDirection::OUTBOUND, 0x01,
                          out_buffer.get(), out_buffer->size(),
                          5000,  // 5 second timeout
                          out_completion.GetCallback());
  out_completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, out_completion.status());
  EXPECT_EQ(static_cast<size_t>(out_buffer->size()),
            out_completion.transferred());

  in_completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, in_completion.status());
  EXPECT_EQ(static_cast<size_t>(in_buffer->size()),
            in_completion.transferred());
  for (size_t i = 0; i < in_completion.transferred(); ++i) {
    EXPECT_EQ(out_buffer->data()[i], in_buffer->data()[i])
        << "Mismatch at index " << i << ".";
  }

  TestResultCallback release_interface;
  handle->ReleaseInterface(0, release_interface.GetCallback());
  ASSERT_TRUE(release_interface.WaitForResult());

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, BulkTransfer) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  EXPECT_FALSE(handle->FindInterfaceByEndpoint(0x81));
  EXPECT_FALSE(handle->FindInterfaceByEndpoint(0x01));
  const UsbInterfaceDescriptor* interface =
      handle->FindInterfaceByEndpoint(0x82);
  EXPECT_TRUE(interface);
  EXPECT_EQ(1, interface->interface_number);
  interface = handle->FindInterfaceByEndpoint(0x02);
  EXPECT_TRUE(interface);
  EXPECT_EQ(1, interface->interface_number);

  scoped_refptr<net::IOBufferWithSize> in_buffer(
      new net::IOBufferWithSize(512));
  TestCompletionCallback in_completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x82, in_buffer.get(),
                          in_buffer->size(),
                          5000,  // 5 second timeout
                          in_completion.GetCallback());

  scoped_refptr<net::IOBufferWithSize> out_buffer(
      new net::IOBufferWithSize(in_buffer->size()));
  TestCompletionCallback out_completion;
  for (int i = 0; i < out_buffer->size(); ++i) {
    out_buffer->data()[i] = i;
  }

  handle->GenericTransfer(UsbTransferDirection::OUTBOUND, 0x02,
                          out_buffer.get(), out_buffer->size(),
                          5000,  // 5 second timeout
                          out_completion.GetCallback());
  out_completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, out_completion.status());
  EXPECT_EQ(static_cast<size_t>(out_buffer->size()),
            out_completion.transferred());

  in_completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, in_completion.status());
  EXPECT_EQ(static_cast<size_t>(in_buffer->size()),
            in_completion.transferred());
  for (size_t i = 0; i < in_completion.transferred(); ++i) {
    EXPECT_EQ(out_buffer->data()[i], in_buffer->data()[i])
        << "Mismatch at index " << i << ".";
  }

  TestResultCallback release_interface;
  handle->ReleaseInterface(1, release_interface.GetCallback());
  ASSERT_TRUE(release_interface.WaitForResult());

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, ControlTransfer) {
  if (!UsbTestGadget::IsTestEnabled())
    return;

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  scoped_refptr<net::IOBufferWithSize> buffer(new net::IOBufferWithSize(255));
  TestCompletionCallback completion;
  handle->ControlTransfer(
      UsbTransferDirection::INBOUND, UsbControlTransferType::STANDARD,
      UsbControlTransferRecipient::DEVICE, 0x06, 0x0301, 0x0409, buffer,
      buffer->size(), 0, completion.GetCallback());
  completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, completion.status());
  const char expected_str[] = "\x18\x03G\0o\0o\0g\0l\0e\0 \0I\0n\0c\0.\0";
  EXPECT_EQ(sizeof(expected_str) - 1, completion.transferred());
  for (size_t i = 0; i < completion.transferred(); ++i) {
    EXPECT_EQ(expected_str[i], buffer->data()[i]) << "Mismatch at index " << i
                                                  << ".";
  }

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, SetInterfaceAlternateSetting) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(2, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  TestResultCallback set_interface;
  handle->SetInterfaceAlternateSetting(2, 1, set_interface.GetCallback());
  ASSERT_TRUE(set_interface.WaitForResult());

  TestResultCallback release_interface;
  handle->ReleaseInterface(2, release_interface.GetCallback());
  ASSERT_TRUE(release_interface.WaitForResult());

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, CancelOnClose) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  scoped_refptr<net::IOBufferWithSize> buffer(new net::IOBufferWithSize(512));
  TestCompletionCallback completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x82, buffer.get(),
                          buffer->size(),
                          5000,  // 5 second timeout
                          completion.GetCallback());

  handle->Close();
  completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::CANCELLED, completion.status());
}

TEST_F(UsbDeviceHandleTest, ErrorOnDisconnect) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  scoped_refptr<net::IOBufferWithSize> buffer(new net::IOBufferWithSize(512));
  TestCompletionCallback completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x82, buffer.get(),
                          buffer->size(),
                          5000,  // 5 second timeout
                          completion.GetCallback());

  ASSERT_TRUE(gadget->Disconnect());
  completion.WaitForResult();
  // Depending on timing the transfer can be cancelled by the disconnection, be
  // rejected because the device is already missing or result in another generic
  // error as the device drops off the bus.
  EXPECT_TRUE(completion.status() == UsbTransferStatus::CANCELLED ||
              completion.status() == UsbTransferStatus::DISCONNECT ||
              completion.status() == UsbTransferStatus::TRANSFER_ERROR);

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, Timeout) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  scoped_refptr<net::IOBufferWithSize> buffer(new net::IOBufferWithSize(512));
  TestCompletionCallback completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x82, buffer.get(),
                          buffer->size(),
                          10,  // 10 millisecond timeout
                          completion.GetCallback());

  completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::TIMEOUT, completion.status());

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, CloseReentrancy) {
  if (!UsbTestGadget::IsTestEnabled())
    return;

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  base::RunLoop run_loop;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(512);
  handle->GenericTransfer(
      UsbTransferDirection::INBOUND, 0x82, buffer.get(), buffer->size(),
      10,  // 10 millisecond timeout
      base::Bind(&ExpectTimeoutAndClose, handle, run_loop.QuitClosure()));
  // Drop handle so that the completion callback holds the last reference.
  handle = nullptr;
  run_loop.Run();
}

}  // namespace

}  // namespace device
