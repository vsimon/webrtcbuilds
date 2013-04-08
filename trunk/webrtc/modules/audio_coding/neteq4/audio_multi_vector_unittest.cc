/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq4/audio_multi_vector.h"

#include <assert.h>
#include <stdlib.h>

#include <string>

#include "gtest/gtest.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// This is a value-parameterized test. The test cases are instantiated with
// different values for the test parameter, which is used to determine the
// number of channels in the AudioMultiBuffer. Note that it is not possible
// to combine typed testing with value-parameterized testing, and since the
// tests for AudioVector already covers a number of different type parameters,
// this test focuses on testing different number of channels, and keeping the
// value type constant.
class AudioMultiVectorTest : public ::testing::TestWithParam<size_t> {
 protected:
  typedef int16_t T;  // Use this value type for all tests.

  AudioMultiVectorTest()
      : num_channels_(GetParam()),  // Get the test parameter.
        interleaved_length_(num_channels_ * kLength) {
    array_interleaved_ = new T[num_channels_ * kLength];
  }

  ~AudioMultiVectorTest() {
    delete [] array_interleaved_;
  }

  virtual void SetUp() {
    // Populate test arrays.
    for (size_t i = 0; i < kLength; ++i) {
      array_[i] = static_cast<T>(i);
    }
    T* ptr = array_interleaved_;
    // Write 100, 101, 102, ... for first channel.
    // Write 200, 201, 202, ... for second channel.
    // And so on.
    for (size_t i = 0; i < kLength; ++i) {
      for (size_t j = 1; j <= num_channels_; ++j) {
        *ptr = j * 100 + i;
        ++ptr;
      }
    }
  }

  enum {
    kLength = 10
  };

  const size_t num_channels_;
  size_t interleaved_length_;
  T array_[kLength];
  T* array_interleaved_;
};

// Create and destroy AudioMultiVector objects, both empty and with a predefined
// length.
TEST_P(AudioMultiVectorTest, CreateAndDestroy) {
  AudioMultiVector<T> vec1(num_channels_);
  EXPECT_TRUE(vec1.Empty());
  EXPECT_EQ(num_channels_, vec1.Channels());
  EXPECT_EQ(0u, vec1.Size());

  size_t initial_size = 17;
  AudioMultiVector<T> vec2(num_channels_, initial_size);
  EXPECT_FALSE(vec2.Empty());
  EXPECT_EQ(num_channels_, vec2.Channels());
  EXPECT_EQ(initial_size, vec2.Size());
}

// Test the subscript operator [] for getting and setting.
TEST_P(AudioMultiVectorTest, SubscriptOperator) {
  AudioMultiVector<T> vec(num_channels_, kLength);
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < kLength; ++i) {
      vec[channel][i] = static_cast<T>(i);
      // Make sure to use the const version.
      const AudioVector<T>& audio_vec = vec[channel];
      EXPECT_EQ(static_cast<T>(i), audio_vec[i]);
    }
  }
}

// Test the PushBackInterleaved method and the CopyFrom method. The Clear
// method is also invoked.
TEST_P(AudioMultiVectorTest, PushBackInterleavedAndCopy) {
  AudioMultiVector<T> vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_, interleaved_length_);
  AudioMultiVector<T> vec_copy(num_channels_);
  vec.CopyFrom(&vec_copy);  // Copy from |vec| to |vec_copy|.
  ASSERT_EQ(num_channels_, vec.Channels());
  ASSERT_EQ(kLength, vec.Size());
  ASSERT_EQ(num_channels_, vec_copy.Channels());
  ASSERT_EQ(kLength, vec_copy.Size());
  for (size_t channel = 0; channel < vec.Channels(); ++channel) {
    for (size_t i = 0; i < kLength; ++i) {
      EXPECT_EQ(static_cast<T>((channel + 1) * 100 + i), vec[channel][i]);
      EXPECT_EQ(vec[channel][i], vec_copy[channel][i]);
    }
  }

  // Clear |vec| and verify that it is empty.
  vec.Clear();
  EXPECT_TRUE(vec.Empty());

  // Now copy the empty vector and verify that the copy becomes empty too.
  vec.CopyFrom(&vec_copy);
  EXPECT_TRUE(vec_copy.Empty());
}

// Try to copy to a NULL pointer. Nothing should happen.
TEST_P(AudioMultiVectorTest, CopyToNull) {
  AudioMultiVector<T> vec(num_channels_);
  AudioMultiVector<T>* vec_copy = NULL;
  vec.PushBackInterleaved(array_interleaved_, interleaved_length_);
  vec.CopyFrom(vec_copy);
}

// Test the PushBack method with another AudioMultiVector as input argument.
TEST_P(AudioMultiVectorTest, PushBackVector) {
  AudioMultiVector<T> vec1(num_channels_, kLength);
  AudioMultiVector<T> vec2(num_channels_, kLength);
  // Set the first vector to [0, 1, ..., kLength - 1] + 100 * channel_number.
  // Set the second vector to [kLength, kLength + 1, ..., 2 * kLength - 1]  +
  // 100 * channel_number.
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < kLength; ++i) {
      vec1[channel][i] = static_cast<T>(i + 100 * channel);
      vec2[channel][i] = static_cast<T>(i + 100 * channel + kLength);
    }
  }
  // Append vec2 to the back of vec1.
  vec1.PushBack(vec2);
  ASSERT_EQ(2u * kLength, vec1.Size());
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < 2 * kLength; ++i) {
      EXPECT_EQ(static_cast<T>(i + 100 * channel), vec1[channel][i]);
    }
  }
}

// Test the PushBackFromIndex method.
TEST_P(AudioMultiVectorTest, PushBackFromIndex) {
  AudioMultiVector<T> vec1(num_channels_);
  vec1.PushBackInterleaved(array_interleaved_, interleaved_length_);
  AudioMultiVector<T> vec2(num_channels_);

  // Append vec1 to the back of vec2 (which is empty). Read vec1 from the second
  // last element.
  vec2.PushBackFromIndex(vec1, kLength - 2);
  ASSERT_EQ(2u, vec2.Size());
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < 2; ++i) {
      EXPECT_EQ(array_interleaved_[channel + num_channels_ * (kLength - 2 + i)],
                vec2[channel][i]);
    }
  }
}

// Starts with pushing some values to the vector, then test the Zeros method.
TEST_P(AudioMultiVectorTest, Zeros) {
  AudioMultiVector<T> vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_, interleaved_length_);
  vec.Zeros(2 * kLength);
  ASSERT_EQ(num_channels_, vec.Channels());
  ASSERT_EQ(2u * kLength, vec.Size());
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < 2 * kLength; ++i) {
      EXPECT_EQ(0, vec[channel][i]);
    }
  }
}

// Test the ReadInterleaved method
TEST_P(AudioMultiVectorTest, ReadInterleaved) {
  AudioMultiVector<T> vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_, interleaved_length_);
  T* output = new T[interleaved_length_];
  // Read 5 samples.
  size_t read_samples = 5;
  EXPECT_EQ(num_channels_ * read_samples,
            vec.ReadInterleaved(read_samples, output));
  EXPECT_EQ(0, memcmp(array_interleaved_, output, read_samples * sizeof(T)));

  // Read too many samples. Expect to get all samples from the vector.
  EXPECT_EQ(interleaved_length_,
            vec.ReadInterleaved(kLength + 1, output));
  EXPECT_EQ(0, memcmp(array_interleaved_, output, read_samples * sizeof(T)));

  delete [] output;
}

// Try to read to a NULL pointer. Expected to return 0.
TEST_P(AudioMultiVectorTest, ReadInterleavedToNull) {
  AudioMultiVector<T> vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_, interleaved_length_);
  T* output = NULL;
  // Read 5 samples.
  size_t read_samples = 5;
  EXPECT_EQ(0u, vec.ReadInterleaved(read_samples, output));
}

// Test the PopFront method.
TEST_P(AudioMultiVectorTest, PopFront) {
  AudioMultiVector<T> vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_, interleaved_length_);
  vec.PopFront(1);  // Remove one element from each channel.
  ASSERT_EQ(kLength - 1u, vec.Size());
  // Let |ptr| point to the second element of the first channel in the
  // interleaved array.
  T* ptr = &array_interleaved_[num_channels_];
  for (size_t i = 0; i < kLength - 1; ++i) {
    for (size_t channel = 0; channel < num_channels_; ++channel) {
      EXPECT_EQ(*ptr, vec[channel][i]);
      ++ptr;
    }
  }
  vec.PopFront(kLength);  // Remove more elements than vector size.
  EXPECT_EQ(0u, vec.Size());
}

// Test the PopBack method.
TEST_P(AudioMultiVectorTest, PopBack) {
  AudioMultiVector<T> vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_, interleaved_length_);
  vec.PopBack(1);  // Remove one element from each channel.
  ASSERT_EQ(kLength - 1u, vec.Size());
  // Let |ptr| point to the first element of the first channel in the
  // interleaved array.
  T* ptr = array_interleaved_;
  for (size_t i = 0; i < kLength - 1; ++i) {
    for (size_t channel = 0; channel < num_channels_; ++channel) {
      EXPECT_EQ(*ptr, vec[channel][i]);
      ++ptr;
    }
  }
  vec.PopBack(kLength);  // Remove more elements than vector size.
  EXPECT_EQ(0u, vec.Size());
}

// Test the AssertSize method.
TEST_P(AudioMultiVectorTest, AssertSize) {
  AudioMultiVector<T> vec(num_channels_, kLength);
  EXPECT_EQ(kLength, vec.Size());
  // Start with asserting with smaller sizes than already allocated.
  vec.AssertSize(0);
  vec.AssertSize(kLength - 1);
  // Nothing should have changed.
  EXPECT_EQ(kLength, vec.Size());
  // Assert with one element longer than already allocated.
  vec.AssertSize(kLength + 1);
  // Expect vector to have grown.
  EXPECT_EQ(kLength + 1u, vec.Size());
  // Also check the individual AudioVectors.
  for (size_t channel = 0; channel < vec.Channels(); ++channel) {
    EXPECT_EQ(kLength + 1u, vec[channel].Size());
  }
}

// Test the PushBack method with another AudioMultiVector as input argument.
TEST_P(AudioMultiVectorTest, OverwriteAt) {
  AudioMultiVector<T> vec1(num_channels_);
  vec1.PushBackInterleaved(array_interleaved_, interleaved_length_);
  AudioMultiVector<T> vec2(num_channels_);
  vec2.Zeros(3);  // 3 zeros in each channel.
  // Overwrite vec2 at position 5.
  vec1.OverwriteAt(vec2, 3, 5);
  // Verify result.
  ASSERT_EQ(kLength, vec1.Size());  // Length remains the same.
  T* ptr = array_interleaved_;
  for (size_t i = 0; i < kLength - 1; ++i) {
    for (size_t channel = 0; channel < num_channels_; ++channel) {
      if (i >= 5 && i <= 7) {
        // Elements 5, 6, 7 should have been replaced with zeros.
        EXPECT_EQ(0, vec1[channel][i]);
      } else {
        EXPECT_EQ(*ptr, vec1[channel][i]);
      }
      ++ptr;
    }
  }
}

INSTANTIATE_TEST_CASE_P(TestNumChannels,
                        AudioMultiVectorTest,
                        ::testing::Values(static_cast<size_t>(1),
                                          static_cast<size_t>(2),
                                          static_cast<size_t>(5)));
}  // namespace webrtc
