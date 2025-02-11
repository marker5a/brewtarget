/*
 * xml/XmlRecord.h is part of Brewtarget, and is Copyright the following
 * authors 2020-2023
 * - Matt Young <mfsy@yahoo.com>
 *
 * Brewtarget is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Brewtarget is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef XML_XMLRECORD_H
#define XML_XMLRECORD_H
#pragma once

#include <memory>

#include <QTextStream>
#include <QVector>

#include <xalanc/DOMSupport/DOMSupport.hpp>
#include <xalanc/XalanDOM/XalanNode.hpp>
#include <xalanc/XPath/NodeRefList.hpp>

#include "model/NamedEntity.h"
#include "model/NamedParameterBundle.h"
#include "utils/EnumStringMapping.h"
#include "utils/ImportRecordCount.h"
#include "utils/TypeLookup.h"
#include "xml/XQString.h"

class XmlCoding;


/**
 * \brief This class and its derived classes represent a record in an XML document.  See comment in xml/XmlCoding.h for
 *        more detail.
 */
class XmlRecord {
public:
   /**
    * At various stages of reading in an XML file, we need to distinguish between three cases:
    *   \c Succeeded - everything went OK and we should continue
    *   \c Failed - there was a problem and we should stop trying to read in the file
    *   \c FoundDuplicate - we realised that the record we are processing is a duplicate of one we already have in the
    *                       DB, in which case we should skip over this record and carry on processing the rest of the
    *                       file
    */
   enum class ProcessingResult {
      Succeeded,
      Failed,
      FoundDuplicate
   };

   /**
    * \brief The types of fields that we know how to process.  Used in \b FieldDefinition records
    */
   enum class FieldType {
      Bool,
      Int,
      UInt,
      Double,
      String,
      Date,
      Enum,              // A string that we need to map to/from our own enum
      RequiredConstant,  // A fixed value we have to write out in the record (used for BeerXML VERSION tag)
      RecordSimple,      // Single contained record
      RecordComplex,     // Zero, one or more contained records
      INVALID
   };

   /**
    * \brief How to parse every field that we want to be able to read out of the XML file.  See class description for
    *        more details.
    */
   struct FieldDefinition {
      FieldType           fieldType;
      XQString            xPath;
      BtStringConst const & propertyName;  // If fieldType == RecordComplex, then this is used only on export
                                           // If fieldType == RequiredConstant, then this is actually the constant value
      EnumStringMapping const * enumMapping; // Only used if fieldType == Enum, otherwise should be nullptr
      FieldDefinition(FieldType           fieldType,
                      XQString            xPath,
                      BtStringConst const & propertyName,
                      EnumStringMapping const * enumMapping = nullptr);
   };

   typedef QVector<FieldDefinition> FieldDefinitions;

   /**
    * \brief Constructor
    * \param recordName The name of the outer tag around this type of record, eg "RECIPE" for a "<RECIPE>...</RECIPE>"
    *                   record in BeerXML.
    * \param xmlCoding An \b XmlCoding object representing the XML Coding we are using (eg BeerXML 1.0).  This is what
    *                  we'll need to look up how to handle nested records inside this one.
    * \param fieldDefinitions A list of fields we expect to find in this record (other fields will be ignored) and how
    *                         to parse them.
    * \param typeLookup The \c TypeLookup object that, amongst other things allows us to tell whether Qt properties on
    *                   this object type are "optional" (ie wrapped in \c std::optional)
    * \param namedEntityClassName The class name of the \c NamedEntity to which this record relates, or empty string if
    *                             there is none
    */
   XmlRecord(QString          const & recordName,
             XmlCoding        const & xmlCoding,
             FieldDefinitions const & fieldDefinitions,
             TypeLookup       const * const typeLookup,
             QString          const & namedEntityClassName);

   // Need a virtual destructor as we have virtual member functions
   virtual ~XmlRecord();

   /**
    * \brief Get the record name (in this coding)
    */
   QString getRecordName() const;

   /**
    * \brief Getter for the NamedParameterBundle we read in from this record
    *
    *        This is needed for the same reasons as \c XmlRecord::getNamedEntity() below
    *
    * \return Reference to an object that the caller does NOT own
    */
   NamedParameterBundle const & getNamedParameterBundle() const;

   /**
    * \brief Getter for the NamedEntity we are reading in from this record
    *
    *        This is needed to allow one \c XmlRecord (or subclass) object to read the data from another (eg for
    *        \c XmlRecipeRecord to work with contained \c XmlRecord objects).  (The protected access on
    *        \c XmlRecord::namedEntity only allows an instance of a derived class to access this field on its own
    *         instance.)
    *
    * \return Shared pointer, which will contain nullptr for the root record
    */
   std::shared_ptr<NamedEntity>  getNamedEntity() const;

   /**
    * \brief From the supplied record (ie node) in an XML document, load into memory the data it contains, including
    *        any other records nested inside it.
    *
    * \param domSupport
    * \param rootNodeOfRecord
    * \param userMessage Where to append any error messages that we want the user to see on the screen
    *
    * \return \b true if load succeeded, \b false if there was an error
    */
   bool load(xalanc::DOMSupport & domSupport,
             xalanc::XalanNode * rootNodeOfRecord,
             QTextStream & userMessage);

   /**
    * \brief Once the record (including all its sub-records) is loaded into memory, we this function does any final
    *        validation and data correction before then storing the object(s) in the database.  Most validation should
    *        already have been done via the XSD, but there are some validation rules have to be done in code, including
    *        checking for duplicates and name clashes.
    *
    *        Child classes may override this function to extend functionality but should make sure to call this base
    *        class version to ensure child nodes are saved.
    *
    * \param containingEntity If not null, this is the entity that contains this one.  Eg, for a MashStep it should
    *                         always be the containing Mash.  For a Style inside a Recipe, this will be a pointer to
    *                         the Recipe, but for a freestanding Style, this will be null.
    * \param userMessage Where to append any error messages that we want the user to see on the screen
    * \param stats This object keeps tally of how many records (of each type) we skipped or stored
    *
    * \return \b Succeeded, if processing succeeded, \b Failed, if there was an unresolvable problem, \b FoundDuplicate
    *         if the current record is a duplicate of one already in the DB and should be skipped.
    */
   virtual ProcessingResult normaliseAndStoreInDb(std::shared_ptr<NamedEntity> containingEntity,
                                                  QTextStream & userMessage,
                                                  ImportRecordCount & stats);
   /**
    * \brief Export to XML
    * \param namedEntityToExport The object that we want to export to XML
    * \param out Where to write the XML
    * \param indentLevel Current number of indents to put before each opening tag (default 1)
    * \param indentString String to use for each indent (default two spaces)
    */
   void toXml(NamedEntity const & namedEntityToExport,
              QTextStream & out,
              int indentLevel = 1,
              char const * const indentString = "  ") const;

private:
   /**
    * \brief Load in child records.  It is for derived classes to determine whether and when they have child records to
    *        process (eg Hop records inside a Recipe).  But the algorithm for processing is generic, so we implement it
    *        in this base class.
    */
   bool loadChildRecords(xalanc::DOMSupport & domSupport,
                         FieldDefinition const * fieldDefinition,
                         xalanc::NodeRefList & nodesForCurrentXPath,
                         QTextStream & userMessage);

protected:
   /**
    * \brief Subclasses need to implement this to populate this->namedEntity with a suitably-constructed object using
    *        the contents of this=>namedParameterBundle
    */
   virtual void constructNamedEntity();

   /**
    * \brief Subclasses  need to implement this to store this->namedEntityRaiiContainer in the appropriate ObjectStore
    * \return the ID of the newly-inserted object
    */
   virtual int storeNamedEntityInDb();

public:
   /**
    * \brief Subclasses  need to implement this to delete this->namedEntityRaiiContainer from the appropriate
    *        ObjectStore (this is in the event of problems detected after the call to this->storeNamedEntityInDb()
    */
   virtual void deleteNamedEntityFromDb();

protected:
   bool normaliseAndStoreChildRecordsInDb(QTextStream & userMessage,
                                          ImportRecordCount & stats);

   /**
    * \brief Checks whether the \c NamedEntity for this record is, in all the ways that count, a duplicate of one we
    *        already have stored in the DB
    *
    *        Note that this is \b not a \c const function as, in the case that we do find a duplicate, we will update
    *        some of our internal data to point to the existing stored \c NamedEntity.
    *
    * \return \b true if this is a duplicate and should be skipped rather than stored
    */
   virtual bool isDuplicate();

   /**
    * \brief If the \c NamedEntity for this record is supposed to have globally unique names, then this method will
    *        check the current name and modify it if necessary.  NB: This function should be called _after_
    *        \b isDuplicate().
    */
   virtual void normaliseName();

   /**
    * \brief If the \b NamedEntity for this record needs to know about its containing entity (because it is owned by
    *        that containing entity), this function should set it - eg this is where a \b BrewNote gets its \b Recipe
    *        set.  For other classes, this function is a no-op.
    */
   virtual void setContainingEntity(std::shared_ptr<NamedEntity> containingEntity);

   /**
    * \brief Called by \c toXml to write out any fields that are themselves records.
    *        Subclasses should provide the obvious recursive implementation.
    * \param fieldDefinition Which of the fields we're trying to export.  It will be of type \c XmlRecord::Record
    * \param subRecord A suitably constructed subclass of \c XmlRecord that can do the export.  (Note that because
    *                  exporting to XML is const on \c XmlRecord, we only need one of these even if there are multiple
    *                  records to export.)
    * \param namedEntityToExport The object containing (or referencing) the data we want to export to XML
    * \param out Where to write the XML
    */
   virtual void subRecordToXml(XmlRecord::FieldDefinition const & fieldDefinition,
                               XmlRecord const & subRecord,
                               NamedEntity const & namedEntityToExport,
                               QTextStream & out,
                               int indentLevel,
                               char const * const indentString) const;

   /**
    * \brief Writes a comment to the XML output when there is no contained record to output (to make it explicit that
    *        the omission was not by accident.
    */
   void writeNone(XmlRecord const & subRecord,
                  NamedEntity const & namedEntityToExport,
                  QTextStream & out,
                  int indentLevel,
                  char const * const indentString) const;

   /**
    * \brief Given a name that is a duplicate of an existing one, modify it to a potential alternative.
    *        Callers should call this function as many times as necessary to find a non-clashing name.
    *
    *        Eg if the supplied clashing name is "Oatmeal Stout", we'll try adding a "duplicate number" in brackets to
    *        the end of the name, ie amending it to "Oatmeal Stout (1)".  If the caller determines that that clashes too
    *        then the next call (supplying "Oatmeal Stout (1)") will make us modify the name to "Oatmeal Stout (2)" (and
    *        NOT "Oatmeal Stout (1) (1)"!).
    *
    * \param candidateName The name that we should attempt to modify.  (Modification is done in place.)
    */
   static void modifyClashingName(QString & candidateName);

   QString const            recordName;
   XmlCoding const &        xmlCoding;
   FieldDefinitions const & fieldDefinitions;
public:

   TypeLookup const * const typeLookup;

   // The name of the class of object contained in this type of record, eg "Hop", "Yeast", etc.
   // Blank for the root record (which is just a container and doesn't have a NamedEntity).
   QString const namedEntityClassName;
protected:
   // Name-value pairs containing all the field data from the XML record that will be used to construct/populate
   // this->namedEntity
   NamedParameterBundle namedParameterBundle;

   //
   // If we created a new NamedEntity (ie Hop/Yeast/Recipe/etc) object to populate with data read in from an XML file,
   // then we need to ensure it is properly destroyed if we abort that processing.  Putting it in this RAII container
   // handles that automatically for us.
   //
   // Once the object is populated, and we give ownership to the relevant Object Store there will be another instance of
   // this shared pointer (in the object store), which is perfect because, at this point, we don't want the new
   // Hop/Yeast/Recipe/etc object to be destroyed when the XmlNamedEntityRecord is destroyed (typically at end of
   // document processing).
   //
   std::shared_ptr<NamedEntity> namedEntity;

   // This determines whether we include this record in the stats we show the user (about how many records were read in
   // or skipped from a file.  By default it's true.  Subclass constructors set it to false for types of record that
   // are entirely owned and contained by other records (eg MashSteps are just part of a Mash, so we tell the user
   // about reading in a Mash but not about reading in a MashStep).
   bool includeInStats;

   //
   // Keep track of any child (ie contained) records
   //
   struct ChildRecord {
      XmlRecord::FieldDefinition const * fieldDefinition;
      std::shared_ptr<XmlRecord> xmlRecord;
   };
   QVector<ChildRecord> childRecords;
};

#endif
