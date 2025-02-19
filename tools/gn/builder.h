// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_BUILDER_H_
#define TOOLS_GN_BUILDER_H_

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "tools/gn/builder_record.h"
#include "tools/gn/label.h"
#include "tools/gn/label_ptr.h"
#include "tools/gn/unique_vector.h"

class Config;
class Err;
class Loader;
class ParseNode;

class Builder : public base::RefCountedThreadSafe<Builder> {
 public:
  typedef base::Callback<void(const BuilderRecord*)> ResolvedCallback;

  explicit Builder(Loader* loader);

  // The resolved callback is called whenever a target has been resolved. This
  // will be executed only on the main thread.
  void set_resolved_callback(const ResolvedCallback& cb) {
    resolved_callback_ = cb;
  }

  Loader* loader() const { return loader_; }

  void ItemDefined(scoped_ptr<Item> item);

  // Returns NULL if there is not a thing with the corresponding label.
  const Item* GetItem(const Label& label) const;
  const Toolchain* GetToolchain(const Label& label) const;

  std::vector<const BuilderRecord*> GetAllRecords() const;

  // Returns targets which should be generated and which are defined.
  std::vector<const Target*> GetAllResolvedTargets() const;

  // Returns the record for the given label, or NULL if it doesn't exist.
  // Mostly used for unit tests.
  const BuilderRecord* GetRecord(const Label& label) const;
  BuilderRecord* GetRecord(const Label& label);

  // If there are any undefined references, returns false and sets the error.
  bool CheckForBadItems(Err* err) const;

 private:
  friend class base::RefCountedThreadSafe<Builder>;

  virtual ~Builder();

  bool TargetDefined(BuilderRecord* record, Err* err);
  bool ToolchainDefined(BuilderRecord* record, Err* err);

  // Returns the record associated with the given label. This function checks
  // that if we already have references for it, the type matches. If no record
  // exists yet, a new one will be created.
  //
  // If any of the conditions fail, the return value will be null and the error
  // will be set. request_from is used as the source of the error.
  BuilderRecord* GetOrCreateRecordOfType(const Label& label,
                                         const ParseNode* request_from,
                                         BuilderRecord::ItemType type,
                                         Err* err);

  // Returns the record associated with the given label. This function checks
  // that it's already been resolved to the correct type.
  //
  // If any of the conditions fail, the return value will be null and the error
  // will be set. request_from is used as the source of the error.
  BuilderRecord* GetResolvedRecordOfType(const Label& label,
                                         const ParseNode* request_from,
                                         BuilderRecord::ItemType type,
                                         Err* err);

  bool AddDeps(BuilderRecord* record,
               const LabelConfigVector& configs,
               Err* err);
  bool AddDeps(BuilderRecord* record,
               const UniqueVector<LabelConfigPair>& configs,
               Err* err);
  bool AddDeps(BuilderRecord* record,
               const LabelTargetVector& targets,
               Err* err);
  bool AddToolchainDep(BuilderRecord* record,
                       const Target* target,
                       Err* err);

  // Given a target, sets the "should generate" bit and pushes it through the
  // dependency tree. Any time the bit it set, we ensure that the given item is
  // scheduled to be loaded.
  //
  // If the force flag is set, we'll ignore the current state of the record's
  // should_generate flag, and set it on the dependents every time. This is
  // used when defining a target: the "should generate" may have been set
  // before the item was defined (if it is required by something that is
  // required). In this case, we need to re-push the "should generate" flag
  // to the item's dependencies.
  void RecursiveSetShouldGenerate(BuilderRecord* record, bool force);

  void ScheduleItemLoadIfNecessary(BuilderRecord* record);

  // This takes a BuilderRecord with resolved depdencies, and fills in the
  // target's Label*Vectors with the resolved pointers.
  bool ResolveItem(BuilderRecord* record, Err* err);

  // Fills in the pointers in the given vector based on the labels. We assume
  // that everything should be resolved by this point, so will return an error
  // if anything isn't found or if the type doesn't match.
  bool ResolveDeps(LabelTargetVector* deps, Err* err);
  bool ResolveConfigs(UniqueVector<LabelConfigPair>* configs, Err* err);
  bool ResolveForwardDependentConfigs(Target* target, Err* err);
  bool ResolveToolchain(Target* target, Err* err);

  // Given a list of unresolved records, tries to find any circular
  // dependencies and returns the string describing the problem. If no circular
  // deps were found, returns the empty string.
  std::string CheckForCircularDependencies(
      const std::vector<const BuilderRecord*>& bad_records) const;

  // Non owning pointer.
  Loader* loader_;

  // Owning pointers.
  typedef base::hash_map<Label, BuilderRecord*> RecordMap;
  RecordMap records_;

  ResolvedCallback resolved_callback_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

#endif  // TOOLS_GN_BUILDER_H_
